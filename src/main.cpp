/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <curl/curl.h>
#include <string>
#include <iostream>
#include <future>
#if defined(_WIN32) and defined(fopen)
	#undef fopen
#endif
#include <simdjson.h>
#include <toml++/toml.h>
#include <src/config_path.hpp>

/*
 * Two handles for two theads.
 * One for cdn-cgi/trace and one for DNS IP
 */

std::size_t write_data(char* incoming_buffer, const std::size_t size, const std::size_t count, std::string* data) {
	data->append(incoming_buffer, size * count);
	return size * count;
}

std::string get_local_ip() {
	// Creating the handle and the response buffer
	CURL* local_ip_handle {curl_easy_init()};
	std::string local_ip_response;

	// General curl options
	curl_easy_setopt(local_ip_handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(local_ip_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(local_ip_handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(local_ip_handle, CURLOPT_WRITEDATA, &local_ip_response);
	curl_easy_setopt(local_ip_handle, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(local_ip_handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(local_ip_handle, CURLOPT_DEFAULT_PROTOCOL, "https");
	curl_easy_setopt(local_ip_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
	
	// Setting up a GET request
	curl_easy_setopt(local_ip_handle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(local_ip_handle, CURLOPT_URL, "https://1.1.1.1/cdn-cgi/trace");

	// Performing the request
	curl_easy_perform(local_ip_handle);
	
	// Cleaning up the handle as I won't reuse it
	curl_easy_cleanup(local_ip_handle);

	// Parsing the response
	const std::size_t ip_begin {local_ip_response.find("ip=") + 3};  // + 3 because "ip=" is 3 chars
	const std::size_t ip_end {local_ip_response.find('\n', ip_begin)};
	return local_ip_response.substr(ip_begin, ip_end - ip_begin);
}

// NOT thread-safe, writes into a buffer and uses an handle both owned elsewhere
void get_dns_record_response(CURL** curl_handle, const std::string_view request_uri) {
	curl_easy_setopt(*curl_handle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(*curl_handle, CURLOPT_URL, request_uri.data());
	curl_easy_perform(*curl_handle);
}

int main(int argc, char* argv[]) {
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
	struct curl_slist* manual_doh_address {nullptr};
	manual_doh_address = curl_slist_append(manual_doh_address, "cloudflare-dns.com:443:104.16.248.249,104.16.249.249,2606:4700::6810:f8f9,2606:4700::6810:f9f9");
	curl_easy_setopt(curl_handle, CURLOPT_RESOLVE, manual_doh_address);
	curl_easy_setopt(curl_handle, CURLOPT_DOH_URL, "https://cloudflare-dns.com/dns-query");
	curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
	curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
	curl_easy_setopt(curl_handle, CURLOPT_XOAUTH2_BEARER, api_token.data());
	struct curl_slist* headers {nullptr};
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);

	std::future<void> dns_response_future {std::async(std::launch::async, get_dns_record_response, &curl_handle, std::string{"https://api.cloudflare.com/client/v4/zones/" + zone_id + "/dns_records?type=A,AAAA&name=" + record_name})};
	std::future<std::string> local_ip_future {std::async(std::launch::async, get_local_ip)};

	simdjson::dom::parser parser;
	dns_response_future.wait();
	const simdjson::dom::element parsed {parser.parse(dns_response)};

	std::string local_ip {local_ip_future.get()};

	if (local_ip != static_cast<std::string_view>((*parsed["result"].begin())["content"])) {
		dns_response.clear();
		curl_easy_setopt(curl_handle, CURLOPT_URL, std::string{"https://api.cloudflare.com/client/v4/zones/" + zone_id + "/dns_records/" + std::string{static_cast<std::string_view>((*parsed["result"].begin())["id"])}}.c_str());
		// curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 0L);
		curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "PATCH");
		std::string request {R"({"content": ")" + local_ip + "\"}"};
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, request.c_str());
		curl_easy_perform(curl_handle);
		std::cout << "New IP: " << parser.parse(dns_response)["result"]["content"] << '\n';
	}
	else {
		std::cout << "The DNS is up to date\n";
	}
	curl_easy_cleanup(curl_handle);
	curl_global_cleanup();
}
