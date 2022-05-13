/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "common.hpp"
#include <array>

int main() {
	"update_record"_test = [] {
		std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> local_ip;
		expect(eq(ddns_get_local_ip(false, local_ip.size(), local_ip.data()), DDNS_ERROR_OK));

		std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> record_ip;
		std::array<char, DDNS_RECORD_ID_LENGTH + 1> record_id;
		bool aaaa;
		expect(eq(ddns_get_record(
			test_api_token,
			test_zone_id,
			test_record_name,
			record_ip.size(), record_ip.data(),
			record_id.size(), record_id.data(),
			&aaaa
		), DDNS_ERROR_OK));

		expect(eq(ddns_update_record(
			test_api_token,
			test_zone_id,
			record_id.data(),
			local_ip.data(),
			record_ip.size(), record_ip.data()
		), DDNS_ERROR_OK));

		expect(eq(
			std::string_view{local_ip.data()},
			std::string_view{record_ip.data()}
		));
	};

	"update_record_bad_usage"_test = [] {
		std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> record_ip;
		expect(eq(
			ddns_update_record(
				"an invalid token",
				test_zone_id,
				"a string that is 32 chars looong",
				"1.2.3.4",
				record_ip.size(), record_ip.data()
			), DDNS_ERROR_USAGE
		));

		expect(eq(
			ddns_update_record(
				test_api_token,
				"an invalid zone id",
				"a string that is 32 chars looong",
				"1.2.3.4",
				record_ip.size(), record_ip.data()
			), DDNS_ERROR_USAGE
		));

		expect(eq(
			ddns_update_record(
				test_api_token,
				test_zone_id,
				"a string that is not 32 characters long",
				"1.2.3.4",
				record_ip.size(), record_ip.data()
			), DDNS_ERROR_USAGE
		));

		expect(eq(
			ddns_update_record(
				test_api_token,
				test_zone_id,
				"a string that is 32 chars looong",
				"Ciao a tutti ragazzi e bentornati in questo nuovo video io sono Tachi_107",
				record_ip.size(), record_ip.data()
			), DDNS_ERROR_USAGE
		));
	};
}
