/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "common.hpp"
#include "credentials.hpp"
#include <array>
#include <cstring>
#include <string_view>

int main() {
	"search_zone_id"_test = [] {
		// +1 because of '\0'
		std::array<char, DDNS_ZONE_ID_LENGTH + 1> zone_id;

		expect(eq(
			ddns_search_zone_id(
				test_api_token,
				test_record_name,
				zone_id.size(), zone_id.data()
			),
			DDNS_ERROR_OK
		));

		// ut can't compare C strings
		expect(eq(
			std::string_view{zone_id.data()},
			std::string_view{test_zone_id}
		));
	};

	"search_zone_id_bad_usage"_test = [] {
		std::array<char, DDNS_ZONE_ID_LENGTH + 1> zone_id;

		expect(eq(
			ddns_search_zone_id(
				test_api_token,
				"Ciao a tutti ragazzi e bentornati in questo nuovo video io sono Tachi_107"
				"ed oggi siamo qui nell'aspettatissimo -egh- guerra verso il drago e il wither"
				"una roba che penso che nessuno abbia mai fatto perché è una roba malsana che non riusciremo mai a fare."
				"In pratica, ceh, adesso noi entriamo e ci sarà il nostro bellissimo drago"
				"e uccideremo anche il wither nello stesso momento -non spingetemi- è una roba malsas",
				zone_id.size(), zone_id.data()
			),
			DDNS_ERROR_USAGE
		));
	};
}
