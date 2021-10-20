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

	priv::curl_handle_setup(&local_ip_handle, local_ip_response);

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

std::string get_record_ip_raw(const std::string& api_token, const std::string& zone_id, const std::string& record_name) {
	// Creating the handle and the response buffer
	CURL* record_ip_handle {curl_easy_init()};
	std::string record_ip_response;

	priv::curl_handle_setup(&record_ip_handle, record_ip_response);
	priv::curl_doh_setup(&record_ip_handle);
	priv::curl_auth_setup(&record_ip_handle, api_token);

	// Setting up a GET request
	curl_easy_setopt(record_ip_handle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(record_ip_handle, CURLOPT_URL, std::string{"https://api.cloudflare.com/client/v4/zones/" + zone_id + "/dns_records?type=A,AAAA&name=" + record_name}.c_str());
}

namespace priv {

std::size_t write_data(char* incoming_buffer, const std::size_t size, const std::size_t count, std::string* data) {
	data->append(incoming_buffer, size * count);
	return size * count;
}

void curl_handle_setup(CURL** handle, const std::string& response_buffer) noexcept {
	// General curl options
	curl_easy_setopt(*handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(*handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(*handle, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(*handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(*handle, CURLOPT_DEFAULT_PROTOCOL, "https");
	curl_easy_setopt(*handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
	curl_easy_setopt(*handle, CURLOPT_WRITEFUNCTION, priv::write_data);
	curl_easy_setopt(*handle, CURLOPT_WRITEDATA, &response_buffer);
}

void curl_doh_setup(CURL** handle) noexcept {
	struct curl_slist* manual_doh_address {nullptr};
	manual_doh_address = curl_slist_append(manual_doh_address, "cloudflare-dns.com:443:104.16.248.249,104.16.249.249,2606:4700::6810:f8f9,2606:4700::6810:f9f9");
	curl_easy_setopt(*handle, CURLOPT_RESOLVE, manual_doh_address);
}

} // namespace priv

} // namespace tachi
