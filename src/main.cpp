/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <curl/curl.h>
#include <string>
#include <iostream>
#include <future>
// curl.h redefines fopen on Windows, causing issues.
#if defined(_WIN32) and defined(fopen)
	#undef fopen
#endif
#include <simdjson.h>
#include <toml++/toml.h>
#include <tachi/cloudflare-ddns.hpp>
#include "src/config_path.hpp"

/*
 * Two handles for two theads.
 * One for cdn-cgi/trace and one for DNS IP
 */

std::size_t write_data(char* incoming_buffer, const std::size_t size, const std::size_t count, std::string* data) {
	data->append(incoming_buffer, size * count);
	return size * count;
}

int main(const int argc, const char* const argv[]) {
	std::string api_token;
	std::string zone_id;
	std::string record_name;

	if (argc == 1 || argc == 3) {
		const std::string_view config_file {argc == 1 ? config_path : std::string_view{argv[2]}};
		try {
			const auto config = toml::parse_file(config_file);
			api_token = config["api_token"].value_exact<std::string>().value();
			zone_id = config["zone_id"].value_exact<std::string>().value();
			record_name = config["record_name"].value_exact<std::string>().value();
		}
		catch (const toml::parse_error& err) {
			std::cerr << "Error parsing " << err.source() << ":\n\t" << err.description() << '\n';
			return EXIT_FAILURE;
		}
		catch (const std::bad_optional_access& err) {
			std::cerr << "Config is missing some required values\n";
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
			<< "Bad usage! You can run the program without arguments and load the config in" << config_path 
			<< " or pass the API token, the Zone ID and the DNS record name as arguments\n";
		return EXIT_FAILURE;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	CURL* curl_handle {curl_easy_init()};
	std::string dns_response;
	dns_response.reserve(600);	// Tipical size of largest response (GET to fetch the DNS IP)

	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &dns_response);
	curl_easy_setopt(curl_handle, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(curl_handle, CURLOPT_DEFAULT_PROTOCOL, "https");

	std::future<void> dns_response_future {std::async(
		std::launch::async,
		tachi::get_record_raw,
		api_token, zone_id, record_name, &curl_handle
	)};

	std::future<std::string> local_ip_future {std::async(
		std::launch::async,
		tachi::get_local_ip
	)};

	simdjson::dom::parser parser;
	dns_response_future.wait();
	const simdjson::dom::element parsed {parser.parse(dns_response)};

	std::string local_ip {local_ip_future.get()};

	if (local_ip != static_cast<std::string_view>((*parsed["result"].begin())["content"])) {
		dns_response.clear();
		tachi::update_record_raw(
			api_token,
			zone_id,
			std::string{static_cast<std::string_view>((*parsed["result"].begin())["id"])},
			local_ip,
			&curl_handle
		);
		std::cout << "New IP: " << parser.parse(dns_response)["result"]["content"] << '\n';
	}
	else {
		std::cout << "The DNS is up to date\n";
	}
	curl_easy_cleanup(curl_handle);
	curl_global_cleanup();
}
