/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <tachi/cloudflare-ddns.hpp>

namespace tachi {

std::string get_local_ip() {
	// Creating the handle and the response buffer
	CURL* local_ip_handle {curl_easy_init()};
	std::string local_ip_response;

	// General curl options
	curl_easy_setopt(local_ip_handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(local_ip_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(local_ip_handle, CURLOPT_WRITEFUNCTION, priv::write_data);
	curl_easy_setopt(local_ip_handle, CURLOPT_WRITEDATA, &local_ip_response);
	curl_easy_setopt(local_ip_handle, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(local_ip_handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(local_ip_handle, CURLOPT_DEFAULT_PROTOCOL, "https");
	curl_easy_setopt(local_ip_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);

	// Setting up a GET request
	curl_easy_setopt(local_ip_handle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(local_ip_handle, CURLOPT_URL, "https://1.1.1.1/cdn-cgi/trace");

	// Performing the request
	curl_easy_perform(local_ip_handle);

	// Cleaning up the handle as I won't reuse it
	curl_easy_cleanup(local_ip_handle);

	// Parsing the response
	const std::size_t ip_begin {local_ip_response.find("ip=") + 3};  // + 3 because "ip=" is 3 chars
	const std::size_t ip_end {local_ip_response.find('\n', ip_begin)};
	return local_ip_response.substr(ip_begin, ip_end - ip_begin);
}

std::size_t priv::write_data(char* incoming_buffer, const std::size_t size, const std::size_t count, std::string* data) {
	data->append(incoming_buffer, size * count);
	return size * count;
}

// NOT thread-safe, writes into a buffer and uses an handle both owned elsewhere
void priv::get_request(CURL** curl_handle, const std::string_view request_uri) {
	curl_easy_setopt(*curl_handle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(*curl_handle, CURLOPT_URL, request_uri.data());
	curl_easy_perform(*curl_handle);
}

} // namespace tachi
