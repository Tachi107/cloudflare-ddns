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

std::size_t writeData(char* incomingBuffer, const std::size_t size, const std::size_t count, std::string* data) {
	data->append(incomingBuffer, size * count);
	return size * count;
}

std::string getLocalIp() {
	// Creating the handle and the response buffer
	CURL* localIpHandle {curl_easy_init()};
	std::string localIpResponse;

	// General curl options
	curl_easy_setopt(localIpHandle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(localIpHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(localIpHandle, CURLOPT_WRITEFUNCTION, writeData);
	curl_easy_setopt(localIpHandle, CURLOPT_WRITEDATA, &localIpResponse);
	curl_easy_setopt(localIpHandle, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(localIpHandle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(localIpHandle, CURLOPT_DEFAULT_PROTOCOL, "https");
	curl_easy_setopt(localIpHandle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
	
	// Setting up a GET request
	curl_easy_setopt(localIpHandle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(localIpHandle, CURLOPT_URL, "https://1.1.1.1/cdn-cgi/trace");

	// Performing the request
	curl_easy_perform(localIpHandle);
	
	// Cleaning up the handle as I won't reuse it
	curl_easy_cleanup(localIpHandle);

	// Parsing the response
	const std::size_t ipBegin {localIpResponse.find("ip=") + 3};  // + 3 because "ip=" is 3 chars
	const std::size_t ipEnd {localIpResponse.find('\n', ipBegin)};
	return localIpResponse.substr(ipBegin, ipEnd - ipBegin);
}

// NOT thread-safe, writes into a buffer and uses an handle both owned elsewhere
void getDnsRecordResponse(CURL** curlHandle, const std::string_view requestUri) {
	curl_easy_setopt(*curlHandle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(*curlHandle, CURLOPT_URL, requestUri.data());
	curl_easy_perform(*curlHandle);
}

int main(int argc, char* argv[]) {
	std::string apiToken;
	std::string zoneId;
	std::string recordName;

	if (argc == 1 || argc == 3) {
		const std::string_view configFile {argc == 1 ? configPath : std::string_view{argv[2]}};
		try {
			const auto config = toml::parse_file(configFile);
			apiToken = config["api-token"].value_exact<std::string>().value();
			zoneId = config["zone-id"].value_exact<std::string>().value();
			recordName = config["record-name"].value_exact<std::string>().value();
		}
		catch (const toml::parse_error&) {
			std::cerr << "Error parsing " << configFile << '\n';
			return EXIT_FAILURE;
		}
		catch (const std::bad_optional_access&) {
			std::cerr << "Bad config file\n";
			return EXIT_FAILURE;
		}
	}
	else if (argc == 4) {
		apiToken = argv[1];
		zoneId = argv[2];
		recordName = argv[3];
	}
	else {
		std::cerr 
			<< "Bad usage! You can run the program without arguments and load the config in" << configPath 
			<< " or pass the API token, the Zone ID and the DNS record name as arguments\n";
		return EXIT_FAILURE;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	CURL* curlHandle {curl_easy_init()};
	std::string dnsResponse;
	dnsResponse.reserve(600);	// Tipical size of largest response (GET to fetch the DNS IP)

	curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeData);
	curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &dnsResponse);
	curl_easy_setopt(curlHandle, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(curlHandle, CURLOPT_DEFAULT_PROTOCOL, "https");
	struct curl_slist* manualDohAddress {nullptr};
	manualDohAddress = curl_slist_append(manualDohAddress, "cloudflare-dns.com:443:104.16.248.249,104.16.249.249,2606:4700::6810:f8f9,2606:4700::6810:f9f9");
	curl_easy_setopt(curlHandle, CURLOPT_RESOLVE, manualDohAddress);
	curl_easy_setopt(curlHandle, CURLOPT_DOH_URL, "https://cloudflare-dns.com/dns-query");
	curl_easy_setopt(curlHandle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
	curl_easy_setopt(curlHandle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
	curl_easy_setopt(curlHandle, CURLOPT_XOAUTH2_BEARER, apiToken.data());
	struct curl_slist* headers {nullptr};
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

	std::future<void> dnsResponseFuture {std::async(std::launch::async, getDnsRecordResponse, &curlHandle, std::string{"https://api.cloudflare.com/client/v4/zones/" + zoneId + "/dns_records?type=A,AAAA&name=" + recordName})};
	std::future<std::string> localIpFuture {std::async(std::launch::async, getLocalIp)};

	simdjson::dom::parser parser;
	dnsResponseFuture.wait();
	const simdjson::dom::element parsed {parser.parse(dnsResponse)};

	std::string localIp {localIpFuture.get()};

	if (localIp != static_cast<std::string_view>((*parsed["result"].begin())["content"])) {
		dnsResponse.clear();
		curl_easy_setopt(curlHandle, CURLOPT_URL, std::string{"https://api.cloudflare.com/client/v4/zones/" + zoneId + "/dns_records/" + std::string{static_cast<std::string_view>((*parsed["result"].begin())["id"])}}.c_str());
		// curl_easy_setopt(curlHandle, CURLOPT_NOBODY, 0L);
		curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, "PATCH");
		std::string request {R"({"content": ")" + localIp + "\"}"};
		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, request.c_str());
		curl_easy_perform(curlHandle);
		std::cout << "New IP: " << parser.parse(dnsResponse)["result"]["content"] << '\n';
	}
	else {
		std::cout << "The DNS is up to date\n";
	}
	curl_easy_cleanup(curlHandle);
	curl_global_cleanup();
}
