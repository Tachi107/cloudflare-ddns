/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "common.hpp"

int main() {
	"update_record"_test = [] {
		std::array<char, INET6_ADDRSTRLEN> local_ip;
		expect(eq(tachi_get_local_ip(local_ip.size(), local_ip.data()), 0));

		std::array<char, INET6_ADDRSTRLEN> record_ip;
		std::array<char, 32> record_id;
		expect(eq(tachi_get_record(
			test_api_token.data(),
			test_zone_id.data(),
			test_record_name.data(),
			record_ip.size(), record_ip.data(),
			record_id.size(), record_id.data()
		), 0));

		expect(eq(tachi_update_record(
			test_api_token.data(),
			test_zone_id.data(),
			record_id.data(),
			local_ip.data(),
			record_ip.size(), record_ip.data()
		), 0));

		expect(eq(
			std::string_view{local_ip.data()},
			std::string_view{record_ip.data()}
		));
	};
}
