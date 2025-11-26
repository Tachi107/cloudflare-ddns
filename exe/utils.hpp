/*
 * SPDX-FileCopyrightText: 2025 Toni Melisma
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

inline std::vector<std::string> parse_record_names(std::string_view input) {
	std::vector<std::string> records;
	std::string_view::size_type pos = 0;

	while (pos != std::string_view::npos) {
		std::string_view::size_type next_pos = input.find(',', pos);
		std::string_view current_record_name = input.substr(pos, next_pos - pos);

		// Trim whitespace
		const auto first = current_record_name.find_first_not_of(" \t");
		if (first == std::string_view::npos) {
			// Empty or all-whitespace record, skip
			if (next_pos == std::string_view::npos) {
				break;
			}
			pos = next_pos + 1;
			continue;
		}
		const auto last = current_record_name.find_last_not_of(" \t");
		current_record_name = current_record_name.substr(first, (last - first) + 1);

		records.emplace_back(current_record_name);

		if (next_pos != std::string_view::npos) {
			pos = next_pos + 1;
		}
		else {
			pos = next_pos;
		}
	}
	return records;
}
