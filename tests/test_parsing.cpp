/*
 * SPDX-FileCopyrightText: 2025 Toni Melisma
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "../exe/utils.hpp"
#include "common.hpp"
#include <string>
#include <vector>

int main() {
	"parse_record_names"_test = [] {
		should("parse single record") = [] {
			auto result = parse_record_names("example.com");
			expect(result.size() == 1_ul);
			expect(result[0] == "example.com");
		};

		should("parse two records") = [] {
			auto result = parse_record_names("example.com,sub.example.com");
			expect(result.size() == 2_ul);
			expect(result[0] == "example.com");
			expect(result[1] == "sub.example.com");
		};

		should("parse records with spaces") = [] {
			auto result = parse_record_names(" example.com , sub.example.com ");
			expect(result.size() == 2_ul);
			expect(result[0] == "example.com");
			expect(result[1] == "sub.example.com");
		};

		should("ignore empty records") = [] {
			auto result = parse_record_names("example.com,,sub.example.com");
			expect(result.size() == 2_ul);
			expect(result[0] == "example.com");
			expect(result[1] == "sub.example.com");
		};

		should("ignore whitespace-only records") = [] {
			auto result = parse_record_names("example.com, ,sub.example.com");
			expect(result.size() == 2_ul);
			expect(result[0] == "example.com");
			expect(result[1] == "sub.example.com");
		};

		should("handle trailing comma") = [] {
			auto result = parse_record_names("example.com,");
			expect(result.size() == 1_ul);
			expect(result[0] == "example.com");
		};

		should("handle leading comma") = [] {
			auto result = parse_record_names(",example.com");
			expect(result.size() == 1_ul);
			expect(result[0] == "example.com");
		};

		should("handle trailing comma with spaces") = [] {
			auto result = parse_record_names("example.com, ");
			expect(result.size() == 1_ul);
			expect(result[0] == "example.com");
		};

		should("handle leading comma with spaces") = [] {
			auto result = parse_record_names(" ,example.com");
			expect(result.size() == 1_ul);
			expect(result[0] == "example.com");
		};
	};
}
