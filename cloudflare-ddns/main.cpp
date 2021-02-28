#include <curl/curl.h>
#include <string>
#include <iostream>
#include <simdjson.h>

std::size_t writeData(char* incomingBuffer, std::size_t size, std::size_t count, std::string* data) {
	data->append(incomingBuffer, size * count);
	return size * count;
}

int main(int argc, char* argv[]) {
	if (argc != 4) {
		std::cerr << "Usage: cloudflare-ddns <API token> <Zone ID> <DNS record name>\n";
		return EXIT_FAILURE;
	}
	const std::string_view apiToken {argv[1]};
	const std::string zoneId {argv[2]};
	const std::string recordName {argv[3]};
	
	CURL* curlHandle {curl_easy_init()};
	std::string response;
	response.reserve(600);	// Tipical size of largest response (GET to fetch the DNS IP)
	curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeData);
	curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curlHandle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_URL, "https://1.1.1.1/cdn-cgi/trace");

	curl_easy_perform(curlHandle);
	const std::size_t ipBegin {response.find("ip=") + 3};  // + 3 because "ip=" is 3 chars
	const std::size_t ipEnd {response.find('\n', ipBegin)};
	const std::string localIp {response.substr(ipBegin, ipEnd - ipBegin)};
	response.clear();

	curl_easy_setopt(curlHandle, CURLOPT_URL, std::string{"https://api.cloudflare.com/client/v4/zones/" + zoneId + "/dns_records?type=A&name=" + recordName}.data());
	curl_easy_setopt(curlHandle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
	curl_easy_setopt(curlHandle, CURLOPT_XOAUTH2_BEARER, apiToken.data());
	struct curl_slist* headers {nullptr};
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

	curl_easy_perform(curlHandle);
	simdjson::dom::parser parser;
	const simdjson::dom::element parsed {parser.parse(response)};

	if (localIp != static_cast<std::string_view>((*parsed["result"].begin())["content"])) {
		response.clear();
		curl_easy_setopt(curlHandle, CURLOPT_URL, std::string{"https://api.cloudflare.com/client/v4/zones/" + zoneId + "/dns_records/" + std::string{static_cast<std::string_view>((*parsed["result"].begin())["id"])}}.c_str());
		// curl_easy_setopt(curlHandle, CURLOPT_NOBODY, 0L);
		curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, "PATCH");
		std::string request {R"({"content": ")" + localIp + "\"}"};
		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, request.c_str());
		curl_easy_perform(curlHandle);
		std::cout << "New IP: " << parser.parse(response)["result"]["content"] << '\n';
	}
	else {
		std::cout << "The DNS is up to date\n";
	}
	curl_easy_cleanup(curlHandle);
}
