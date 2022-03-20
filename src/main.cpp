/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <curl/curl.h>
#include <string>
#include <iostream>
#include <future>
#include <array>
// curl.h redefines fopen on Windows, causing issues.
#if defined(_WIN32) and defined(fopen)
	#undef fopen
#endif
#include <simdjson.h>
#include <INIReader.h>
#include <tachi/cloudflare-ddns.h>
#include "config_path.hpp"

/*
 * Two handles for two theads.
 * One for cdn-cgi/trace and one for DNS IP
 */

struct static_buffer {
	static constexpr std::size_t capacity {600}; // Tipical size of largest response (GET to fetch the DNS IP)
	std::size_t size {0};
	char buffer[capacity];
};

static std::size_t write_data(
	char* TACHI_RESTRICT incoming_buffer,
	const std::size_t /*size*/, // size will always be 1
	const std::size_t count,
	static_buffer* TACHI_RESTRICT data
) TACHI_NOEXCEPT {
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

	std::future<int> dns_response_future {std::async(
		std::launch::async,
		tachi_get_record_raw,
		api_token.c_str(), zone_id.c_str(), record_name.c_str(), &curl_handle
	)};

	std::array<char, 46> local_ip;
	std::future<int> local_ip_future {std::async(
		std::launch::async,
		tachi_get_local_ip,
		local_ip.size(), local_ip.data()
	)};

	simdjson::dom::parser parser;

	if (dns_response_future.get() != 0) {
		std::cerr << "Error getting DNS record info\n";
		curl_cleanup(&curl_handle);
		return EXIT_FAILURE;
	}
	const simdjson::dom::element parsed {parser.parse(std::string_view{dns_response.buffer, dns_response.size})};

	if (local_ip_future.get() != 0) {
		std::cerr << "Error getting the local IP address\n";
		curl_cleanup(&curl_handle);
		return EXIT_FAILURE;
	}

	if (std::string_view{local_ip.data()} != static_cast<std::string_view>((*parsed["result"].begin())["content"])) {
		dns_response.size = 0;
		if (tachi_update_record_raw(
			api_token.c_str(),
			zone_id.c_str(),
			static_cast<const char*>((*parsed["result"].begin())["id"]),
			local_ip.data(),
			&curl_handle
		) != 0) {
			std::cerr << "Error updating the DNS record\n";
			curl_cleanup(&curl_handle);
			return EXIT_FAILURE;
		}
		std::cout << "New IP: " << parser.parse(std::string_view{dns_response.buffer, dns_response.size})["result"]["content"] << '\n';
	}
	else {
		std::cout << "The DNS is up to date\n";
	}
	curl_cleanup(&curl_handle);
}
