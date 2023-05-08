/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "common.hpp"
#include <curl/curl.h>
#include <array>

namespace {
extern "C" std::size_t write_data(char* incoming_buffer, const std::size_t size, const std::size_t count, std::string* data) {
	data->append(incoming_buffer, size * count);
	return size * count;
}
}

int main() {
	curl_global_init(CURL_GLOBAL_DEFAULT);

	"get_local_ip"_test = [] {
		std::string response;
		CURL* curl {curl_easy_init()};

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(curl, CURLOPT_URL, "https://icanhazip.com");

		curl_easy_perform(curl);

		response.pop_back(); // remove \n character

		std::array<char, DDNS_IP_ADDRESS_MAX_LENGTH> local_ip;

		expect(eq(ddns_get_local_ip(false, local_ip.size(), local_ip.data()), DDNS_ERROR_OK));

		expect(eq(
			std::string_view{local_ip.data()},
			response
		));
		curl_easy_cleanup(curl);
	};
	curl_global_cleanup();
}
