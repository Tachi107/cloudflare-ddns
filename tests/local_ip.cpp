/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "common.hpp"
#include <curl/curl.h>

extern "C" {
std::size_t write_data(char* incoming_buffer, const std::size_t size, const std::size_t count, std::string* data) {
	data->append(incoming_buffer, size * count);
	return size * count;
}
}

CURL* get_test_curl(std::string& response_buffer) {
	CURL* curl {curl_easy_init()};
	curl_easy_setopt(
		curl,
		CURLOPT_WRITEFUNCTION,
		write_data
	);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
	return curl;
}

int main() {
	curl_global_init(CURL_GLOBAL_DEFAULT);

	"local_ip"_test = [] {
		std::string response;
		CURL* curl {get_test_curl(response)};

		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(curl, CURLOPT_URL, "https://icanhazip.com");
		curl_easy_perform(curl);

		response.pop_back(); // remove \n character

		expect(eq(tachi::get_local_ip(), response));
		curl_easy_cleanup(curl);
	};
	curl_global_cleanup();
}
