/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 * SPDX-FileCopyrightText: 2025 Toni Melisma
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <curl/curl.h>
// curl.h redefines fopen on Windows, causing issues.
#ifdef _WIN32
namespace std {
	static const auto& curlx_win32_fopen = fopen;
}
#endif

#include <array> /* std::array */
#include <cstddef> /* std::size_t */
#include <cstdio> /* std::printf, std::fprintf, std::puts, std::fputs */
#include <cstring> /* std::memchr, std::memcpy */
#include <filesystem> /* std::filesystem::path::preferred_separator */
#include <fstream> /* std::ifstream, std::ofstream */
#include <optional> /* std::optional */
#include <string> /* std::string */
#include <string_view> /* std::string_view */
#include <vector> /* std::vector */

#include <INIReader.h>
#include <ddns/cloudflare-ddns.h>
#include "paths.hpp"
#include "utils.hpp"

struct static_buffer {
	static constexpr std::size_t capacity {CURL_MAX_WRITE_SIZE / 2}; // typical requests are smaller than 2000 bytes
	std::size_t size {0};
	char buffer[capacity];
};

static std::size_t write_data(
	char* DDNS_RESTRICT incoming_buffer,
	const std::size_t /*size*/, // size will always be 1
	const std::size_t count,
	static_buffer* DDNS_RESTRICT data
) DDNS_NOEXCEPT {
	// Check if the static buffer can handle all the new data
	if (data->size + count >= static_buffer::capacity) {
		return 0;
	}
	// Append to the buffer
	std::memcpy(data->buffer + data->size, incoming_buffer, /*size **/ count);
	// Increase the current size
	data->size += /*size **/ count;
	// null-terminate the buffer (so that it can be used as a C string)
	// data->buffer[data->size] = '\0';
	return /*size **/ count;
}

/*
 * Same as the POSIX strnlen():
 * https://pubs.opengroup.org/onlinepubs/9699919799/functions/strnlen.html
 */
static std::size_t ddns_strnlen(const char* const s, const size_t maxlen) {
	const char* end = static_cast<const char*>(std::memchr(s, '\0', maxlen));
	if (end == nullptr) {
		return maxlen;
	}
	return end - s;
}

/*
 * key must include quotes
 */
static std::optional<std::string_view> get_json_value(const std::string_view json, const std::string_view key) {
	const size_t key_start = json.find(key);
	if (key_start == std::string_view::npos) {
		return {};
	}

	const size_t key_end = key_start + key.length();
	if (key_end == std::string_view::npos) {
		return {};
	}

	const size_t value_start = json.find('"', key_end + 1) + 1;
	if (value_start == std::string_view::npos) {
		return {};
	}

	const size_t value_end = json.find('"', value_start + 1);
	if (value_end == std::string_view::npos) {
		return {};
	}

	return std::string_view(json.data() + value_start, value_end - value_start);
}

int main(const int argc, char* argv[]) {
	std::string api_token;
	std::string record_name;

	if (argc == 1 || argc == 3) {
		if (argc == 3 && std::strcmp(argv[1], "--config") != 0) {
			api_token = argv[1];
			record_name = argv[2];
		}
		else {
			const std::string config_file {argc == 1 ? config_path : argv[2]};
			const INIReader reader {config_file};

			if (reader.ParseError() == -1) {
				std::fprintf(stderr, "Unable to open %s\n", config_file.c_str());
				return EXIT_FAILURE;
			}
			else if (int error = reader.ParseError(); error > 0) {
				std::fprintf(stderr, "Error parsing %s on line %d\n", config_file.c_str(), error);
				return EXIT_FAILURE;
			}

			api_token   = reader.GetString("ddns", "api_token",   "token");
			record_name = reader.GetString("ddns", "record_name", "name");

			if (api_token == "token" || record_name == "name") {
				std::fprintf(stderr, "Error parsing %s\n", config_file.c_str());
				return EXIT_FAILURE;
			}
		}
	}
	else {
		std::fprintf(stderr,
			"Bad usage! You can run the program without arguments and load the config in %s "
			"or pass the API token and the DNS record name as arguments\n", config_path.data());
		return EXIT_FAILURE;
	}

	const std::vector<std::string> record_names {parse_record_names(record_name)};

	if (record_names.empty()) {
		std::fputs("No valid record names provided.\n", stderr);
		return EXIT_FAILURE;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	int return_code = EXIT_SUCCESS;

	std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> local_ips[2];
	std::array<bool, 2> local_ip_fetched = {false, false};

	for (const std::string& current_record_name : record_names) {
		// Create a fresh curl handle for each record to avoid state pollution
		// (e.g., CURLOPT_CUSTOMREQUEST from a PATCH persisting into subsequent GETs)
		CURL* curl_handle {curl_easy_init()};
		static_buffer dns_response;

		curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &dns_response);
		curl_easy_setopt(curl_handle, CURLOPT_TCP_FASTOPEN, 1L);
		curl_easy_setopt(curl_handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
		curl_easy_setopt(curl_handle, CURLOPT_DEFAULT_PROTOCOL, "https");
		/* Make sure to avoid path traversal vulnerabilities */
		if (current_record_name.find('/') != std::string::npos
			|| current_record_name.find(std::filesystem::path::preferred_separator) != std::string::npos) {
			std::fprintf(stderr, "[%s] Record names cannot contain path separators!\n", current_record_name.c_str());
			curl_easy_cleanup(curl_handle);
			return_code = EXIT_FAILURE;
			continue;
		}

		ddns_error error = DDNS_ERROR_OK;
		// +1 because of '\0'
		std::array<char, DDNS_ZONE_ID_LENGTH + 1> zone_id;

		const std::string cache_path = std::string{cache_dir} + current_record_name;

		// Here the cache file is opened twice, the first time read-only and
		// the second time write-only. This is because if the filesystem is
		// mounted read-only I'm still able to read the cache, if available.
		const bool cache_miss = std::ifstream{cache_path, std::ios::binary}
			.read(zone_id.data(), zone_id.size())
			.fail();

		if (cache_miss || ddns_strnlen(zone_id.data(), zone_id.size()) != DDNS_ZONE_ID_LENGTH) {
			error = ddns_search_zone_id(api_token.c_str(), current_record_name.c_str(), zone_id.size(), zone_id.data());
			if (error) {
				std::fprintf(stderr, "[%s] Error getting the Zone ID\n", current_record_name.c_str());
				curl_easy_cleanup(curl_handle);
				return_code = EXIT_FAILURE;
				continue;
			}
			// This also writes '\0'
			std::ofstream{cache_path, std::ios::binary}
				.write(zone_id.data(), zone_id.size());
		}

		dns_response.size = 0;
		error = ddns_get_record_raw(api_token.c_str(), zone_id.data(), current_record_name.c_str(), &curl_handle);
		if (error) {
			std::fprintf(stderr,
				"[%s] Error getting DNS record info\n"
				"API response: %.*s\n",
				current_record_name.c_str(),
				static_cast<int>(dns_response.size),
				dns_response.buffer
			);
			curl_easy_cleanup(curl_handle);
			return_code = EXIT_FAILURE;
			continue;
		}

		// The first element contains the IPv4 address, while the second
		// contains the IPv6 one.
		constexpr const char* ipv_c_str[2] = {"IPv4", "IPv6"};
		constexpr const char* type_c_str[2] = {"A", "AAAA"};
		std::string_view record_ids[2];
		std::string_view record_ips[2];
		std::size_t records_count = 0;

		const char* id_pos = dns_response.buffer;
		const char* type_pos = dns_response.buffer;
		const char* dns_ip_pos = dns_response.buffer;
		while (true) {
			const std::optional id = get_json_value(std::string_view(id_pos, dns_response.size - (id_pos - dns_response.buffer)), "\"id\"");
			if (!id.has_value()) {
				break;
			}
			id_pos = id->data();

			const std::optional type = get_json_value(std::string_view(type_pos, dns_response.size - (type_pos - dns_response.buffer)), "\"type\"");
			if (!type.has_value()) {
				break;
			}
			type_pos = type->data();

			const std::optional dns_ip = get_json_value(std::string_view(dns_ip_pos, dns_response.size - (dns_ip_pos - dns_response.buffer)), "\"content\"");
			if (!dns_ip.has_value()) {
				break;
			}
			dns_ip_pos = dns_ip->data();

			if (type == "A") {
				record_ids[0] = *id;
				record_ips[0] = *dns_ip;
				records_count++;
			}
			else if (type == "AAAA") {
				record_ids[1] = *id;
				record_ips[1] = *dns_ip;
				records_count++;
			}
		}

		if (records_count == 0) {
			std::fprintf(stderr, "[%s] doesn't point to any A or AAAA record\n", current_record_name.c_str());
			curl_easy_cleanup(curl_handle);
			return_code = EXIT_FAILURE;
			continue;
		}
		else if (records_count > 2) {
			std::fprintf(stderr, "[%s] points to more than two records, things might not work as expected\n", current_record_name.c_str());
		}

		unsigned int local_ips_count = 0;

		for (unsigned int i = 0; i < 2; i++) {
			if (record_ips[i].empty()) {
				continue;
			}
			if (!local_ip_fetched[i]) {
				error = ddns_get_local_ip(i, local_ips[i].size(), local_ips[i].data());
				if (error) {
					std::fprintf(stderr, "[%s] Error getting the local %s address\n", current_record_name.c_str(), ipv_c_str[i]);
					continue;
				}
				local_ip_fetched[i] = true;
			}
			local_ips_count++;

			if (local_ips[i].data() != record_ips[i]) {
				// record_ids isn't NULL-terminated since it's just a view of
				// dns_response, so I need to create a NULL-termiated C string
				// manually. record_ids.length() and DDNS_RECORD_ID_LENGTH are
				// the same.
				char id[DDNS_RECORD_ID_LENGTH + 1];
				std::memcpy(id, record_ids[i].data(), DDNS_RECORD_ID_LENGTH);
				id[DDNS_RECORD_ID_LENGTH] = '\0';

				dns_response.size = 0;
				if (ddns_update_record_raw(
					api_token.c_str(),
					zone_id.data(),
					id,
					local_ips[i].data(),
					&curl_handle
				) != DDNS_ERROR_OK) {
					std::fprintf(stderr, "[%s] Error updating the %s record\n", current_record_name.c_str(), type_c_str[i]);
					return_code = EXIT_FAILURE;
					continue;
				}

				const std::optional new_ip = get_json_value(std::string_view(dns_response.buffer, dns_response.size), "\"content\"");
				if (!new_ip.has_value()) {
					std::fprintf(stderr, "[%s] Something went very wrong\n", current_record_name.c_str());
					return_code = EXIT_FAILURE;
					continue;
				}

				std::printf("[%s] New %s: %.*s\n",
					current_record_name.c_str(),
					ipv_c_str[i],
					static_cast<int>(new_ip->length()),
					new_ip->data()
				);
			}
			else {
				std::printf("[%s] The %s record is up to date\n", current_record_name.c_str(), type_c_str[i]);
			}
		}

		if (local_ips_count == 0) {
			return_code = EXIT_FAILURE;
		}

		curl_easy_cleanup(curl_handle);
	}

	curl_global_cleanup();

	return return_code;
}