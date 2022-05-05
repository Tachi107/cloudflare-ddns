/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <curl/curl.h>

#include <array> /* std::array */
#include <cstddef> /* std::size_t */
#include <cstring> /* std::memcpy */
#include <iostream> /* std::cout, std::cerr */
#include <string> /* std::string */
#include <string_view> /* std::string_view */

// curl.h redefines fopen on Windows, causing issues.
#if defined(_WIN32) and defined(fopen)
	#undef fopen
#endif
#include <INIReader.h>
#include <simdjson.h>
#include <ddns/cloudflare-ddns.h>
#include "config_path.hpp"

namespace json {
	using parser = simdjson::ondemand::parser;
	using document = simdjson::ondemand::document;
	using object = simdjson::ondemand::object;
	using array = simdjson::ondemand::array;
}

struct static_buffer {
	static constexpr std::size_t capacity {1024}; // Tipical size of largest response (GET to fetch the DNS IP) plus some padding
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
	std::string zone_id;
	std::string record_name;

	if (argc == 1 || argc == 3) {
		const std::string_view config_file {argc == 1 ? config_path : std::string_view{argv[2]}};
		const INIReader reader {std::string{config_file}};

		if (reader.ParseError() == -1) {
			std::cerr << "Unable to open " << config_file << '\n';
			return EXIT_FAILURE;
		}
		else if (int error = reader.ParseError(); error > 0) {
			std::cerr << "Error parsing " << config_file << " on line " << error << '\n';
			return EXIT_FAILURE;
		}

		api_token   = reader.GetString("ddns", "api_token",   "n");
		zone_id     = reader.GetString("ddns", "zone_id",     "n");
		record_name = reader.GetString("ddns", "record_name", "n");

		if (api_token == "n" || zone_id == "n" || record_name == "n") {
			std::cerr << "Error parsing " << config_file << '\n';
			return EXIT_FAILURE;
		}
	}
	else if (argc == 4) {
		api_token = argv[1];
		zone_id = argv[2];
		record_name = argv[3];
	}
	else {
		std::cerr
			<< "Bad usage! You can run the program without arguments and load the config in " << config_path
			<< " or pass the API token, the Zone ID and the DNS record name as arguments\n";
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

	int error = ddns_get_record_raw(api_token.c_str(), zone_id.c_str(), record_name.c_str(), &curl_handle);
	if (error) {
		std::cerr << "Error getting DNS record info\n";
		curl_cleanup(&curl_handle);
		return EXIT_FAILURE;
	}

	json::parser parser;
	json::document json {parser.iterate(dns_response.buffer, dns_response.size, dns_response.capacity)};

	json::array result_arr = json["result"];
	json::object result = *result_arr.begin();

	// parse values in the expected order to improve performance
	const std::string_view id_sv = result["id"];
	const std::string_view type = result["type"];
	const std::string_view content = result["content"];

	const bool ipv6 = (type == "AAAA");

	std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> local_ip;

	error = ddns_get_local_ip(local_ip.size(), local_ip.data(), ipv6);
	if (error) {
		std::cerr << "Error getting the local IP address\n";
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
			zone_id.c_str(),
			id,
			local_ip.data(),
			&curl_handle
		) != 0) {
			std::cerr << "Error updating the DNS record\n";
			curl_cleanup(&curl_handle);
			return EXIT_FAILURE;
		}
		json::document json = parser.iterate(dns_response.buffer, dns_response.size, dns_response.capacity);
		std::cout << "New IP: " << json["result"]["content"] << '\n';
	}
	else {
		std::cout << "The DNS is up to date\n";
	}
	curl_cleanup(&curl_handle);
}
