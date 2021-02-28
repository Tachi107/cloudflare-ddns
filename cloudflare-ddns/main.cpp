#include <curl/curl.h>
#include <string>
#include <iostream>
#include <simdjson.h>

/*
 * Two handles for two theads.
 * One for cdn-cgi/trace and one for DNS IP
 */

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
	
	curl_global_init(CURL_GLOBAL_SSL);

	std::string localIpResponse;
	std::thread localIpThread {[&localIpResponse](){
		CURL* localIpHandle {curl_easy_init()};
		localIpResponse.reserve(150);

		curl_easy_setopt(localIpHandle, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(localIpHandle, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(localIpHandle, CURLOPT_WRITEFUNCTION, writeData);
		curl_easy_setopt(localIpHandle, CURLOPT_WRITEDATA, &localIpResponse);
		curl_easy_setopt(localIpHandle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
		curl_easy_setopt(localIpHandle, CURLOPT_DEFAULT_PROTOCOL, "https");
		curl_easy_setopt(localIpHandle, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(localIpHandle, CURLOPT_DOH_URL, "https://cloudflare-dns.com/dns-query");
		struct curl_slist* manualDohAddress {nullptr};
		manualDohAddress = curl_slist_append(manualDohAddress, "cloudflare-dns.com:443:104.16.248.249,104.16.249.249,2606:4700::6810:f8f9,2606:4700::6810:f9f9");
		curl_easy_setopt(localIpHandle, CURLOPT_RESOLVE, manualDohAddress);
		curl_easy_setopt(localIpHandle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
		curl_easy_setopt(localIpHandle, CURLOPT_TCP_FASTOPEN, 1L);
		curl_easy_setopt(localIpHandle, CURLOPT_URL, "https://1.1.1.1/cdn-cgi/trace");
		
		curl_easy_perform(localIpHandle);
		
		curl_easy_cleanup(localIpHandle);
	}};

	CURL* curlHandle {curl_easy_init()};
	std::string dnsResponse;
	dnsResponse.reserve(600);	// Tipical size of largest response (GET to fetch the DNS IP)

	curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeData);
	curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &dnsResponse);
	curl_easy_setopt(curlHandle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(curlHandle, CURLOPT_DEFAULT_PROTOCOL, "https");
	curl_easy_setopt(curlHandle, CURLOPT_DOH_URL, "https://cloudflare-dns.com/dns-query");
	struct curl_slist* manualDohAddress {nullptr};
	manualDohAddress = curl_slist_append(manualDohAddress, "cloudflare-dns.com:443:104.16.248.249,104.16.249.249,2606:4700::6810:f8f9,2606:4700::6810:f9f9");
	curl_easy_setopt(curlHandle, CURLOPT_RESOLVE, manualDohAddress);
	curl_easy_setopt(curlHandle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
	curl_easy_setopt(curlHandle, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
	curl_easy_setopt(curlHandle, CURLOPT_XOAUTH2_BEARER, apiToken.data());
	struct curl_slist* headers {nullptr};
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

	curl_easy_setopt(curlHandle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_URL, std::string{"https://api.cloudflare.com/client/v4/zones/" + zoneId + "/dns_records?type=A&name=" + recordName}.data());
	curl_easy_perform(curlHandle);

	localIpThread.join();

	const std::size_t ipBegin {localIpResponse.find("ip=") + 3};  // + 3 because "ip=" is 3 chars
	const std::size_t ipEnd {localIpResponse.find('\n', ipBegin)};
	const std::string localIp {localIpResponse.substr(ipBegin, ipEnd - ipBegin)};
	localIpResponse.clear();

	simdjson::dom::parser parser;
	const simdjson::dom::element parsed {parser.parse(dnsResponse)};

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
