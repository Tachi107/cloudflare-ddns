/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
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
#include <string> /* std::string */
#include <string_view> /* std::string_view */

#include <INIReader.h>
#include <simdjson.h>
#include <ddns/cloudflare-ddns.h>
#include "paths.hpp"

namespace json {
	using simdjson::ondemand::parser;
	using simdjson::ondemand::document;
	using simdjson::ondemand::object;
	using simdjson::ondemand::array;
}

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

static void curl_cleanup(CURL** curl) {
	curl_easy_cleanup(*curl);
	curl_global_cleanup();
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

int main(const int argc, const char* const argv[]) {
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

	/* Make sure to avoid path traversal vulnerabilities */
	if (record_name.find('/') != std::string::npos
		|| record_name.find(std::filesystem::path::preferred_separator) != std::string::npos) {
		std::fputs("Record names cannot contain path separators!\n", stderr);
		return EXIT_FAILURE;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	CURL* curl_handle {curl_easy_init()};
	static_buffer dns_response;

	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &dns_response);
	curl_easy_setopt(curl_handle, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(curl_handle, CURLOPT_DEFAULT_PROTOCOL, "https");

	ddns_error error = DDNS_ERROR_OK;
	// +1 because of '\0'
	std::array<char, DDNS_ZONE_ID_LENGTH + 1> zone_id;

	const std::string cache_path = std::string{cache_dir} + record_name;

	// Here the cache file is opened twice, the first time read-only and
	// the second time write-only. This is because if the filesystem is
	// mounted read-only I'm still able to read the cache, if available.
	const bool cache_miss = std::ifstream{cache_path, std::ios::binary}
		.read(zone_id.data(), zone_id.size())
		.fail();

	if (cache_miss || ddns_strnlen(zone_id.data(), zone_id.size()) != DDNS_ZONE_ID_LENGTH) {
		error = ddns_search_zone_id(api_token.c_str(), record_name.c_str(), zone_id.size(), zone_id.data());
		if (error) {
			std::fputs("Error getting the Zone ID\n", stderr);
			curl_cleanup(&curl_handle);
			return EXIT_FAILURE;
		}
		// This also writes '\0'
		std::ofstream{cache_path, std::ios::binary}
			.write(zone_id.data(), zone_id.size());
	}

	error = ddns_get_record_raw(api_token.c_str(), zone_id.data(), record_name.c_str(), &curl_handle);
	if (error) {
		std::fprintf(stderr,
			"Error getting DNS record info\n"
			"API response: %.*s\n", static_cast<int>(dns_response.size), dns_response.buffer);
		curl_cleanup(&curl_handle);
		return EXIT_FAILURE;
	}

	json::parser parser;
	json::document json {parser.iterate(dns_response.buffer, dns_response.size, dns_response.capacity)};

	// The first element contains the IPv4 address, while the second
	// contains the IPv6 one.
	constexpr const char* ipv_c_str[2] = {"IPv4", "IPv6"};
	constexpr const char* type_c_str[2] = {"A", "AAAA"};
	std::string_view record_ids[2];
	std::string_view record_ips[2];
	std::size_t records_count = 0;

	for (json::object result : json["result"]) {
		// parse values in the expected order to improve performance
		const std::string_view id = result["id"];
		const std::string_view type = result["type"];
		const std::string_view dns_ip = result["content"];

		if (type == "A") {
			record_ids[0] = id;
			record_ips[0] = dns_ip;
			records_count++;
		}
		else if (type == "AAAA") {
			record_ids[1] = id;
			record_ips[1] = dns_ip;
			records_count++;
		}
	}

	if (records_count == 0) {
		std::fprintf(stderr, "%s doesn't point to any A or AAAA record\n", record_name.c_str());
		curl_cleanup(&curl_handle);
		return EXIT_FAILURE;
	}
	else if (records_count > 2) {
		std::fprintf(stderr, "%s points to more than two records, things might not work as expected\n", record_name.c_str());
	}

	std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> local_ips[2];

	unsigned int local_ips_count = 0;

	for (unsigned int i = 0; i < 2; i++) {
		if (record_ips[i].empty()) {
			continue;
		}
		error = ddns_get_local_ip(i, local_ips[i].size(), local_ips[i].data());
		if (error) {
			std::fprintf(stderr, "Error getting the local %s address\n", ipv_c_str[i]);
			continue;
		}
		local_ips_count++;

		if (local_ips[i].data() != record_ips[i]) {
			// simdjson ondemand doesn't have get_c_str(), so I need to create
			// a NULL-termiated C string manually. record_ids.length() and
			// DDNS_RECORD_ID_LENGTH are the same.
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
				std::fprintf(stderr, "Error updating the %s record\n", type_c_str[i]);
				curl_cleanup(&curl_handle);
				return EXIT_FAILURE;
			}
			json::document json = parser.iterate(dns_response.buffer, dns_response.size, dns_response.capacity);
			const std::string_view new_ip = json["result"]["content"];
			std::printf("New %s: %.*s\n", ipv_c_str[i], static_cast<int>(new_ip.length()), new_ip.data());
		}
		else {
			std::printf("The %s record is up to date\n", type_c_str[i]);
		}
	}

	curl_cleanup(&curl_handle);

	if (local_ips_count == 0) {
		return EXIT_FAILURE;
	}
}
