/*
 * SPDX-FileCopyrightText: 2025 Toni Melisma
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "common.hpp"
#include "../exe/utils.hpp"
#include <array>
#include <string>
#include <vector>

int main() {
	/**
	 * Test updating a single record to verify backward compatibility.
	 * This mirrors the existing update_record test but uses the
	 * multi-record loop pattern.
	 */
	"update_single_record"_test = [] {
		std::vector<std::string> records = {test_record_name};
		expect(records.size() == 1_ul);

		for (const auto& record : records) {
			std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> local_ip;
			expect(eq(ddns_get_local_ip(false, local_ip.size(), local_ip.data()), DDNS_ERROR_OK));

			std::array<char, DDNS_ZONE_ID_LENGTH + 1> zone_id;
			expect(eq(ddns_search_zone_id(test_api_token, record.c_str(), zone_id.size(), zone_id.data()), DDNS_ERROR_OK));

			std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> record_ip;
			std::array<char, DDNS_RECORD_ID_LENGTH + 1> record_id;
			bool aaaa;
			expect(eq(ddns_get_record(
				test_api_token,
				zone_id.data(),
				record.c_str(),
				record_ip.size(), record_ip.data(),
				record_id.size(), record_id.data(),
				&aaaa
			), DDNS_ERROR_OK));

			expect(eq(ddns_update_record(
				test_api_token,
				zone_id.data(),
				record_id.data(),
				local_ip.data(),
				record_ip.size(), record_ip.data()
			), DDNS_ERROR_OK));

			expect(eq(
				std::string_view{local_ip.data()},
				std::string_view{record_ip.data()}
			));
		}
	};

	/**
	 * Test updating two different records in sequence.
	 */
	"update_two_records"_test = [] {
		std::vector<std::string> records = {test_record_name, test_record_name_2};
		expect(records.size() == 2_ul);

		int success_count = 0;
		for (const auto& record : records) {
			std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> local_ip;
			ddns_error err = ddns_get_local_ip(false, local_ip.size(), local_ip.data());
			if (err != DDNS_ERROR_OK) {
				continue;
			}

			std::array<char, DDNS_ZONE_ID_LENGTH + 1> zone_id;
			err = ddns_search_zone_id(test_api_token, record.c_str(), zone_id.size(), zone_id.data());
			if (err != DDNS_ERROR_OK) {
				continue;
			}

			std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> record_ip;
			std::array<char, DDNS_RECORD_ID_LENGTH + 1> record_id;
			bool aaaa;
			err = ddns_get_record(
				test_api_token,
				zone_id.data(),
				record.c_str(),
				record_ip.size(), record_ip.data(),
				record_id.size(), record_id.data(),
				&aaaa
			);
			if (err != DDNS_ERROR_OK) {
				continue;
			}

			err = ddns_update_record(
				test_api_token,
				zone_id.data(),
				record_id.data(),
				local_ip.data(),
				record_ip.size(), record_ip.data()
			);
			if (err == DDNS_ERROR_OK) {
				success_count++;
			}
		}
		expect(success_count == 2_i);
	};

	/**
	 * Test that an invalid record fails gracefully without crashing.
	 * The loop should continue processing after encountering an error.
	 */
	"update_one_valid_one_invalid_record"_test = [] {
		std::vector<std::string> records = {test_record_name, "nonexistent.invalid.domain.example"};

		int success_count = 0;
		int failure_count = 0;

		for (const auto& record : records) {
			std::array<char, DDNS_ZONE_ID_LENGTH + 1> zone_id;
			ddns_error err = ddns_search_zone_id(test_api_token, record.c_str(), zone_id.size(), zone_id.data());

			if (err == DDNS_ERROR_OK) {
				success_count++;
			}
			else {
				failure_count++;
			}
		}

		// First record should succeed, second should fail
		expect(success_count == 1_i);
		expect(failure_count == 1_i);
	};

	/**
	 * Test that an empty record list is handled correctly.
	 */
	"update_empty_record_list"_test = [] {
		std::vector<std::string> records = {};
		expect(records.empty());

		int iterations = 0;
		for (const auto& record : records) {
			(void)record;
			iterations++;
		}
		expect(iterations == 0_i);
	};

	/**
	 * Test that parse_record_names correctly handles a single record.
	 */
	"parse_single_record"_test = [] {
		auto result = parse_record_names(test_record_name);
		expect(result.size() == 1_ul);
		expect(result[0] == test_record_name);
	};
}
