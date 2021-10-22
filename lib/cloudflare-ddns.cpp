/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <tachi/cloudflare-ddns.hpp>
#include <string_view>
#include <simdjson.h>

namespace tachi {

std::string get_local_ip() {
	// Creating the handle and the response buffer
	CURL* curl {curl_easy_init()};
	std::string response;

	priv::curl_handle_setup(&curl, response);
	priv::curl_get_setup(&curl, "https://1.1.1.1/cdn-cgi/trace");

	// Performing the request
	curl_easy_perform(curl);

	// Cleaning up the handle as I won't reuse it
	curl_easy_cleanup(curl);

	// Parsing the response
	const std::size_t ip_begin {response.find("ip=") + 3};  // + 3 because "ip=" is 3 chars
	const std::size_t ip_end {response.find('\n', ip_begin)};
	return response.substr(ip_begin, ip_end - ip_begin);
}

std::pair<const std::string, const std::string> get_record(const std::string& api_token, const std::string& zone_id, const std::string& record_name) {
	const std::string record_raw {get_record_raw(api_token, zone_id, record_name)};
	simdjson::dom::parser parser;

	const simdjson::dom::element parsed {parser.parse(record_raw)};

	const std::string_view record_ip {(*parsed["result"].begin())["content"]};
	const std::string_view record_id {(*parsed["result"].begin())["id"]};

	return std::make_pair(std::string{record_ip}, std::string{record_id});
}

std::string get_record_raw(const std::string& api_token, const std::string& zone_id, const std::string& record_name) {
	// Creating the handle and the response buffer
	CURL* curl {curl_easy_init()};
	std::string response;

	priv::curl_handle_setup(&curl, response);
	priv::curl_doh_setup(&curl);
	priv::curl_auth_setup(&curl, api_token.c_str());
	priv::curl_get_setup(&curl, std::string{"https://api.cloudflare.com/client/v4/zones/" + zone_id + "/dns_records?type=A,AAAA&name=" + record_name}.c_str());

	curl_easy_perform(curl);

	curl_easy_cleanup(curl);

	return response;
}

void get_record_raw(const std::string& api_token, const std::string& zone_id, const std::string& record_name, CURL** curl) {
	priv::curl_doh_setup(curl);
	priv::curl_auth_setup(curl, api_token.c_str());
	priv::curl_get_setup(curl, std::string{"https://api.cloudflare.com/client/v4/zones/" + zone_id + "/dns_records?type=A,AAAA&name=" + record_name}.c_str());

	curl_easy_perform(*curl);
}

std::string update_record(const std::string &api_token, const std::string &zone_id, const std::string &record_id, const std::string& new_ip) {
	CURL* curl {curl_easy_init()};
	std::string response;

	priv::curl_handle_setup(&curl, response);
	priv::curl_doh_setup(&curl);
	priv::curl_auth_setup(&curl, api_token.c_str());
	priv::curl_patch_setup(
		&curl,
		std::string{"https://api.cloudflare.com/client/v4/zones/" + zone_id + "/dns_records/" + record_id}.c_str(),
		std::string{R"({"content": ")" + new_ip + "\"}"}.c_str()
	);

	curl_easy_perform(curl);

	curl_easy_cleanup(curl);

	simdjson::dom::parser parser;

	return std::string{static_cast<std::string_view>(
		parser.parse(response)["result"]["content"])
	};
}

void update_record_raw(const std::string &api_token, const std::string &zone_id, const std::string &record_id, const std::string &new_ip, CURL **curl) {
	priv::curl_doh_setup(curl);
	priv::curl_auth_setup(curl, api_token.c_str());
	priv::curl_patch_setup(
		curl,
		std::string{"https://api.cloudflare.com/client/v4/zones/" + zone_id + "/dns_records/" + record_id}.c_str(),
		std::string{R"({"content": ")" + new_ip + "\"}"}.c_str()
	);

	curl_easy_perform(*curl);
}

namespace priv {

std::size_t write_data(char* incoming_buffer, const std::size_t size, const std::size_t count, std::string* data) {
	data->append(incoming_buffer, size * count);
	return size * count;
}

void curl_handle_setup(CURL** curl, const std::string& response_buffer) noexcept {
	// General curl options
	curl_easy_setopt(*curl, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(*curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(*curl, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(*curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(*curl, CURLOPT_DEFAULT_PROTOCOL, "https");
	curl_easy_setopt(*curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
	curl_easy_setopt(*curl, CURLOPT_WRITEFUNCTION, priv::write_data);
	curl_easy_setopt(*curl, CURLOPT_WRITEDATA, &response_buffer);
}

void curl_doh_setup(CURL** curl) noexcept {
	struct curl_slist* manual_doh_address {nullptr};
	manual_doh_address = curl_slist_append(manual_doh_address, "cloudflare-dns.com:443:104.16.248.249,104.16.249.249,2606:4700::6810:f8f9,2606:4700::6810:f9f9");
	curl_easy_setopt(*curl, CURLOPT_RESOLVE, manual_doh_address);
	curl_easy_setopt(*curl, CURLOPT_DOH_URL, "https://cloudflare-dns.com/dns-query");
}

void curl_auth_setup(CURL** curl, const char* const api_token) noexcept {
	curl_easy_setopt(*curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
	curl_easy_setopt(*curl, CURLOPT_XOAUTH2_BEARER, api_token);
	struct curl_slist* headers {nullptr};
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, headers);
}

void curl_get_setup(CURL** curl, const char* const url) noexcept {
	curl_easy_setopt(*curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(*curl, CURLOPT_URL, url);
}

void curl_patch_setup(CURL** curl, const char* const url, const char* const body) noexcept {
	curl_easy_setopt(*curl, CURLOPT_CUSTOMREQUEST, "PATCH");
	curl_easy_setopt(*curl, CURLOPT_URL, url);
	curl_easy_setopt(*curl, CURLOPT_POSTFIELDS, body);
}

} // namespace priv

} // namespace tachi
