#include "common.hpp"

int main() {
	"update_record"_test = [] {
		auto local_ip {tachi::get_local_ip()};

		auto[record_ip, record_id] {
			tachi::get_record(
				std::string{test_api_token},
				std::string{test_zone_id},
				std::string{test_record_name}
			)
		};

		auto new_ip {
			tachi::update_record(
				std::string{test_api_token},
				std::string{test_zone_id},
				record_id,
				local_ip
			)
		};

		expect(eq(local_ip, new_ip));
	};
}
