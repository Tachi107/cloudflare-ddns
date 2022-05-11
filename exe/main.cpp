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
#include <cstring> /* std::memcpy */
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

void curl_cleanup(CURL** curl) {
	curl_easy_cleanup(*curl);
	curl_global_cleanup();
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

	// Here the cache file is opened twice, the first time read-only and
	// the second time write-only. This is because if the filesystem is
	// mounted read-only I'm still able to read the cache, if available.
	bool fail_read = std::ifstream{cache_path.data(), std::ios::binary}.read(zone_id.data(), zone_id.size()).fail();

	if (fail_read || std::strlen(zone_id.data()) != DDNS_ZONE_ID_LENGTH) {
		error = ddns_search_zone_id(api_token.c_str(), record_name.c_str(), zone_id.size(), zone_id.data());
		if (error) {
			std::fputs("Error getting the Zone ID\n", stderr);
			curl_cleanup(&curl_handle);
			return EXIT_FAILURE;
		}
		std::ofstream{cache_path.data(), std::ios::binary}.write(zone_id.data(), zone_id.size());
	}

	error = ddns_get_record_raw(api_token.c_str(), zone_id.data(), record_name.c_str(), &curl_handle);
	if (error) {
		std::fputs("Error getting DNS record info\n", stderr);
		curl_cleanup(&curl_handle);
		return EXIT_FAILURE;
	}

	json::parser parser;
	json::document json {parser.iterate(dns_response.buffer, dns_response.size, dns_response.capacity)};

	json::array result_arr = json["result"];
	json::object result = result_arr.at(0);

	// parse values in the expected order to improve performance
	const std::string_view id_sv = result["id"];
	const std::string_view type = result["type"];
	const std::string_view content = result["content"];

	const bool ipv6 = (type == "AAAA");

	std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> local_ip;

	error = ddns_get_local_ip(ipv6, local_ip.size(), local_ip.data());
	if (error) {
		std::fputs("Error getting the local IP address\n", stderr);
		curl_cleanup(&curl_handle);
		return EXIT_FAILURE;
	}

	if (local_ip.data() != content) {
		// simdjson ondemand doesn't have get_c_str(), so I need to create
		// a NULL-termiated C string manually. id_sv.length() and
		// DDNS_RECORD_ID_LENGTH are the same.
		char id[DDNS_RECORD_ID_LENGTH + 1];
		std::memcpy(id, id_sv.data(), DDNS_RECORD_ID_LENGTH);
		id[DDNS_RECORD_ID_LENGTH] = '\0';

		dns_response.size = 0;
		if (ddns_update_record_raw(
			api_token.c_str(),
			zone_id.data(),
			id,
			local_ip.data(),
			&curl_handle
		) != DDNS_ERROR_OK) {
			std::fputs("Error updating the DNS record\n", stderr);
			curl_cleanup(&curl_handle);
			return EXIT_FAILURE;
		}
		json::document json = parser.iterate(dns_response.buffer, dns_response.size, dns_response.capacity);
		const std::string_view new_ip = json["result"]["content"];
		std::printf("New IP: %.*s\n", static_cast<int>(new_ip.length()), new_ip.data());
	}
	else {
		std::puts("The DNS is up to date");
	}
	curl_cleanup(&curl_handle);
}
