#include <cpr/cpr.h>
#include <curl/curl.h>
#include <string>
#include <simdjson.h>
#include <nlohmann/json.hpp>

void libcurlSucks() {
	curl_global_init(CURL_GLOBAL_SSL);

	CURL* curlHandle {curl_easy_init()};
	curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_HTTPGET, 1L);

	curl_global_cleanup();
}

int main(int argc, char* argv[]) {
	if (argc != 4) {
		std::cerr << "Usage: cloudflare-ddns <API token> <Zone ID> <DNS record name>\n";
		return EXIT_FAILURE;
	}
	const std::string apiToken {argv[1]};
	const std::string zoneId {argv[2]};
	const std::string recordName {argv[3]};

	simdjson::dom::parser parser;
	const simdjson::dom::element parsed {
		parser.parse(
			cpr::Get(
				cpr::Url{"https://api.cloudflare.com/client/v4/zones/" + zoneId + "/dns_records?type=A&name=" + recordName},
				cpr::Header{
					{"Content-Type", "application/json"},
					{"Authorization", "Bearer " + apiToken}
				}
				// cpr::Bearer{apiToken} in cpr 1.6
			).text
		)
	};

	const std::string trace {cpr::Get(cpr::Url{"https://1.1.1.1/cdn-cgi/trace"}).text};
	const std::size_t ipBegin {trace.find("ip=") + 3}; // + 3 because "ip=" is 3 chars
	const std::size_t ipEnd {trace.find('\n', ipBegin)};
	const std::string_view currentIp {std::string_view{trace}.substr(ipBegin, ipEnd - ipBegin)};  // Begin, length

	if (currentIp != static_cast<std::string_view>((*parsed["result"].begin())["content"])) {
		std::cout << "New IP: " << (parser.parse(cpr::Patch(
			cpr::Url{"https://api.cloudflare.com/client/v4/zones/" + zoneId + "/dns_records/" + std::string{static_cast<std::string_view>((*parsed["result"].begin())["id"])}},
			cpr::Header{
				{"Content-Type", "application/json"},
				{"Authorization", "Bearer " + apiToken}
			},
			cpr::Body{nlohmann::json{{"content", currentIp}}.dump()}
		).text))["result"]["content"] << '\n';
	}
	else {
		std::cout << "The DNS is up to date\n";
	}
}
