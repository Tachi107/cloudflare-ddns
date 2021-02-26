#include <cpr/cpr.h>
#include <string>
#include <simdjson.h>
#include <nlohmann/json.hpp>

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

	std::string currentIp {cpr::Get(cpr::Url{"https://icanhazip.com"}).text};
	currentIp.pop_back();	// Remove the \n

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
