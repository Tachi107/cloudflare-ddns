/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <tachi/cloudflare-ddns.h>
// curl.h redefines fopen on Windows, causing issues.
#if defined(fopen) && (fopen == curlx_win32_fopen)
namespace std {
	const auto& curlx_win32_fopen = fopen;
}
#endif
#include <string>
#include <string_view>
#include <cstring>
#include <simdjson.h>
#include <curl/curl.h>

extern "C" {

namespace priv {

static std::size_t write_data(char* incoming_buffer, const std::size_t size, const std::size_t count, std::string* data) {
	data->append(incoming_buffer, size * count);
	return size * count;
}

static void curl_handle_setup(CURL** curl, const std::string& response_buffer) noexcept {
	// General curl options
	curl_easy_setopt(*curl, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(*curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(*curl, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(*curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
	curl_easy_setopt(*curl, CURLOPT_DEFAULT_PROTOCOL, "https");
	curl_easy_setopt(*curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
	curl_easy_setopt(*curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(*curl, CURLOPT_WRITEDATA, &response_buffer);
}

static void curl_doh_setup(CURL** curl) noexcept {
	struct curl_slist* manual_doh_address {nullptr};
	manual_doh_address = curl_slist_append(manual_doh_address, "cloudflare-dns.com:443:104.16.248.249,104.16.249.249,2606:4700::6810:f8f9,2606:4700::6810:f9f9");
	curl_easy_setopt(*curl, CURLOPT_RESOLVE, manual_doh_address);
	curl_easy_setopt(*curl, CURLOPT_DOH_URL, "https://cloudflare-dns.com/dns-query");
}

static void curl_auth_setup(CURL** curl, const char* const api_token) noexcept {
	curl_easy_setopt(*curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
	curl_easy_setopt(*curl, CURLOPT_XOAUTH2_BEARER, api_token);
	struct curl_slist* headers {nullptr};
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, headers);
}

static void curl_get_setup(CURL** curl, const char* const url) noexcept {
	curl_easy_setopt(*curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(*curl, CURLOPT_URL, url);
}

static void curl_patch_setup(CURL** curl, const char* const url, const char* const body) noexcept {
	curl_easy_setopt(*curl, CURLOPT_CUSTOMREQUEST, "PATCH");
	curl_easy_setopt(*curl, CURLOPT_URL, url);
	curl_easy_setopt(*curl, CURLOPT_POSTFIELDS, body);
}

} // namespace priv

int tachi_get_local_ip(size_t ip_size, char* ip) {
	// Creating the handle and the response buffer
	CURL* curl {curl_easy_init()};
	std::string response;

	priv::curl_handle_setup(&curl, response);
	priv::curl_doh_setup(&curl);
	priv::curl_get_setup(&curl, "https://one.one.one.one/cdn-cgi/trace");

	// Performing the request
	curl_easy_perform(curl);

	// Cleaning up the handle as I won't reuse it
	curl_easy_cleanup(curl);

	// Parsing the response
	const std::size_t ip_begin {response.find("ip=") + 3U};  // + 3 because "ip=" is 3 chars
	const std::size_t ip_end {response.find('\n', ip_begin)};
	const std::size_t ip_length {ip_end - ip_begin};

	if (ip_length >= ip_size) {
		return 2;
	}

	// Copying the ip in the caller's buffer
	// Using memcpy because I don't need to copy the whole response
	std::memcpy(ip, response.c_str() + ip_begin, ip_length);
	ip[ip_length] = '\0';

	return 0;
}


int tachi_get_record(const char* api_token, const char* zone_id, const char* record_name, size_t record_ip_size, char* record_ip, size_t record_id_size, char* record_id) {
	CURL* curl {curl_easy_init()};
	std::string response;

	priv::curl_handle_setup(&curl, response);

	tachi_get_record_raw(api_token, zone_id, record_name, &curl);

	curl_easy_cleanup(curl);

	simdjson::dom::parser parser;

	const simdjson::dom::element parsed {parser.parse(response)};

	std::string_view record_ip_sv;
	if ((*parsed["result"].begin())["content"].get(record_ip_sv) != simdjson::SUCCESS) {
		return 1;
	}

	std::string_view record_id_sv;
	if ((*parsed["result"].begin())["id"].get(record_id_sv) != simdjson::SUCCESS) {
		return 1;
	}

	if (record_ip_sv.size() >= record_ip_size || record_id_sv.size() >= record_id_size) {
		return 2;
	}

	std::memcpy(record_ip, record_ip_sv.data(), record_ip_sv.size());
	record_ip[record_ip_sv.size()] = '\0';

	std::memcpy(record_id, record_id_sv.data(), record_id_sv.size());
	record_id[record_id_sv.size()] = '\0';

	return 0;
}

int tachi_get_record_raw(const char* api_token, const char* zone_id, const char* record_name, void** curl) {
	priv::curl_doh_setup(curl);
	priv::curl_auth_setup(curl, api_token);

	constexpr std::string_view base_url {"https://api.cloudflare.com/client/v4/zones/"};
	constexpr std::string_view dns_records_url {"/dns_records?type=A,AAAA&name="};

	// Could use string_view instead of c_str + size. Should investigate performance
	const std::size_t zone_id_length {std::strlen(zone_id)};
	const std::size_t record_name_length {std::strlen(record_name)};
	// -1 because sizeof also counts '\0'
	const std::size_t request_url_length {
		base_url.size() +
		zone_id_length +
		dns_records_url.size() +
		record_name_length
	};

	// +1 because of '\0', allocating because the size is not known at compile time
	char* request_url {new char[request_url_length + 1]};

	// Concatenate strings
	std::memcpy(request_url, base_url.data(), base_url.size());
	std::memcpy(request_url + base_url.size(), zone_id, zone_id_length);
	std::memcpy(request_url + base_url.size() + zone_id_length, dns_records_url.data(), dns_records_url.size());
	std::memcpy(request_url + base_url.size() + zone_id_length + dns_records_url.size(), record_name, record_name_length);
	request_url[request_url_length] = '\0';

	priv::curl_get_setup(curl, request_url);

	curl_easy_perform(*curl);

	delete[] request_url;

	return 0;
}
/*

std::string update_record(const std::string &api_token, const std::string &zone_id, const std::string &record_id, const std::string& new_ip) {
	CURL* curl {curl_easy_init()};
	std::string response;

	priv::curl_handle_setup(&curl, response);

	update_record_raw(api_token, zone_id, record_id, new_ip, &curl);

	curl_easy_cleanup(curl);

	simdjson::dom::parser parser;

	return std::string{static_cast<std::string_view>(
		parser.parse(response)["result"]["content"])
	};
}

void update_record_raw(const std::string &api_token, const std::string &zone_id, const std::string &record_id, const std::string &new_ip, CURL** curl) {
	priv::curl_doh_setup(curl);
	priv::curl_auth_setup(curl, api_token.c_str());

	// This request buffer needs to be valid when calling curl_easy_perform()
	std::string request {R"({"content": ")" + new_ip + "\"}"};
	priv::curl_patch_setup(
		curl,
		std::string{"https://api.cloudflare.com/client/v4/zones/" + zone_id + "/dns_records/" + record_id}.c_str(),
		request.c_str()
	);

	curl_easy_perform(*curl);
}
*/
} // extern "C"
