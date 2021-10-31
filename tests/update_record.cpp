/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "common.hpp"

int main() {
	"update_record"_test = [] {
		const auto local_ip {tachi::get_local_ip()};

		const auto[record_ip, record_id] {
			tachi::get_record(
				std::string{test_api_token},
				std::string{test_zone_id},
				std::string{test_record_name}
			)
		};

		const auto new_ip {
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
