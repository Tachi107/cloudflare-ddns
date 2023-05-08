/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

/*
 * Define DDNS_BUILDING_DLL in the .cpp file so that DDNS_PUB correctly
 * expands to __declspec(dllexport)
 */
#if defined _WIN32 && defined DDNS_SHARED_LIB
#	define DDNS_BUILDING_DLL
#endif

#include <ddns/cloudflare-ddns.h>

#include <curl/curl.h>
// curl.h redefines fopen on Windows, causing issues.
#ifdef _WIN32
namespace std {
	static const auto& curlx_win32_fopen = fopen;
}
#endif

#include <cstring> /* std::memcpy, std::size_t, std::strlen */
#include <string_view> /* std::string_view */

#include <simdjson.h>

/*
 * By reading Cloudflare's docs, I can see what is the maximum allowed
 * length of every parameter. With this knowledge, I can statically
 * figure out how much memory I need to make my requests, and I can
 * thus avoid dynamic memory allocations when creating the request URLs
 */

extern "C" {

namespace json {
	using simdjson::ondemand::parser;
	using simdjson::ondemand::document;
	using simdjson::ondemand::object;
	using simdjson::ondemand::array;
	using simdjson::error_code::SUCCESS;
}

namespace {

constexpr std::string_view base_url {"https://api.cloudflare.com/client/v4/zones/"};

struct static_buffer {
	static constexpr std::size_t capacity {CURL_MAX_WRITE_SIZE};
	std::size_t size {0};
	char buffer[capacity];
};

std::size_t write_data(
	char* DDNS_RESTRICT incoming_buffer,
	const std::size_t /*size*/, // size will always be 1
	const std::size_t count,
	static_buffer* DDNS_RESTRICT data
) DDNS_NOEXCEPT {
	// Check if the static buffer can handle all the new data
	if (data->size + count >= static_buffer::capacity) {
		return 0;
	}
	// Append to the buffer
	std::memcpy(data->buffer + data->size, incoming_buffer, /*size **/ count);
	// Increase the current size
	data->size += /*size **/ count;
	// null-terminate the buffer (so that it can be used as a C string)
	// data->buffer[data->size] = '\0';
	return /*size **/ count;
}

void curl_handle_setup(
	CURL** DDNS_RESTRICT curl,
	const static_buffer& response_buffer
) DDNS_NOEXCEPT {
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

/*
 * Returns the curl_slist that must be freed with curl_slist_free_all()
 */
DDNS_NODISCARD curl_slist* curl_doh_setup([[maybe_unused]] CURL** DDNS_RESTRICT curl) DDNS_NOEXCEPT {
#if LIBCURL_VERSION_NUM >= 0x073e00
	struct curl_slist* manual_doh_address {nullptr};
	manual_doh_address = curl_slist_append(manual_doh_address, "cloudflare-dns.com:443:104.16.248.249,104.16.249.249,2606:4700::6810:f8f9,2606:4700::6810:f9f9");
	curl_easy_setopt(*curl, CURLOPT_RESOLVE, manual_doh_address);
	curl_easy_setopt(*curl, CURLOPT_DOH_URL, "https://cloudflare-dns.com/dns-query");
	return manual_doh_address;
#else
	return nullptr;
#endif
}

DDNS_NODISCARD curl_slist* curl_auth_setup(CURL** DDNS_RESTRICT curl, const char* DDNS_RESTRICT const api_token) DDNS_NOEXCEPT {
	curl_easy_setopt(*curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
	//curl_easy_setopt(*curl, CURLOPT_XOAUTH2_BEARER, api_token); leaks, see https://github.com/curl/curl/issues/8841

	// -1 because I have to overwrite the '\0'
	constexpr std::size_t bearer_length = sizeof "Authorization: Bearer " - 1;
	char bearer[bearer_length + DDNS_API_TOKEN_LENGTH + 1] = "Authorization: Bearer ";
	std::memcpy(bearer + bearer_length, api_token, DDNS_API_TOKEN_LENGTH);
	bearer[bearer_length + DDNS_API_TOKEN_LENGTH] = '\0';

	struct curl_slist* headers {nullptr};
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, bearer);
	curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, headers);
	return headers;
}

void curl_get_setup(CURL** DDNS_RESTRICT curl, const char* DDNS_RESTRICT const url) DDNS_NOEXCEPT {
	curl_easy_setopt(*curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(*curl, CURLOPT_URL, url);
}

void curl_patch_setup(CURL** DDNS_RESTRICT curl, const char* DDNS_RESTRICT const url, const char* DDNS_RESTRICT const body) DDNS_NOEXCEPT {
	curl_easy_setopt(*curl, CURLOPT_CUSTOMREQUEST, "PATCH");
	curl_easy_setopt(*curl, CURLOPT_URL, url);
	curl_easy_setopt(*curl, CURLOPT_POSTFIELDS, body);
}

} // namespace

DDNS_NODISCARD DDNS_PUB ddns_error ddns_get_local_ip(
	const bool ipv6,
	const size_t ip_size, char* DDNS_RESTRICT ip
) DDNS_NOEXCEPT {
	// Creating the handle and the response buffer
	CURL* curl {curl_easy_init()};
	static_buffer response;

	curl_handle_setup(&curl, response);
	curl_slist* free_me {curl_doh_setup(&curl)};
	curl_get_setup(&curl, "https://one.one.one.one/cdn-cgi/trace");

	if (ipv6) {
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
	}
	else {
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	}

	// Performing the request
	const int curl_error = curl_easy_perform(curl);

	// Cleaning up the headers
	curl_slist_free_all(free_me);

	// Cleaning up the handle as I won't reuse it
	curl_easy_cleanup(curl);

	if (curl_error) {
		// I can return as I've cleaned up everything
		return DDNS_ERROR_GENERIC;
	}

	const std::string_view response_sv {response.buffer, response.size};
	// Parsing the response
	const std::size_t ip_begin {response_sv.find("ip=") + 3U};  // + 3 because "ip=" is 3 chars
	const std::size_t ip_end {response_sv.find('\n', ip_begin)};
	const std::size_t ip_length {ip_end - ip_begin};

	if (ip_length >= ip_size) {
		return DDNS_ERROR_USAGE;
	}

	// Copying the ip in the caller's buffer
	// Using memcpy because I don't need to copy the whole response
	std::memcpy(ip, response.buffer + ip_begin, ip_length);
	ip[ip_length] = '\0';

	return DDNS_ERROR_OK;
}

DDNS_NODISCARD DDNS_PUB ddns_error ddns_search_zone_id(
	const char* const DDNS_RESTRICT api_token,
	const char* const DDNS_RESTRICT record_name,
	const size_t zone_id_size, char* DDNS_RESTRICT zone_id
) DDNS_NOEXCEPT {
	// <= because I also have to write '\0'
	if (zone_id_size <= DDNS_ZONE_ID_LENGTH) {
		return DDNS_ERROR_USAGE;
	}

	std::string_view record_name_sv = record_name;

	CURL* curl = curl_easy_init();
	static_buffer response;

	curl_handle_setup(&curl, response);

	json::parser parser;

	std::string_view zone_id_sv;

	bool found = false;

	for (auto pos = record_name_sv.find_first_of('.'); !found && pos != std::string_view::npos; pos = record_name_sv.find_first_of('.')) {
		// before making a new request I have to reset the current buffer size
		response.size = 0;

		const ddns_error error = ddns_get_zone_id_raw(api_token, record_name_sv.data(), &curl);

		// +1 because I also need to remove the leading dot
		record_name_sv.remove_prefix(pos + 1);

		if (error == DDNS_ERROR_USAGE) {
			// a usage error will make the request fail over and over again
			curl_easy_cleanup(curl);
			return error;
		}
		else if (error) {
			// here continue acts as "retry"
			continue;
		}

		json::document json;
		if (parser.iterate(response.buffer, response.size, response.capacity).get(json) != json::SUCCESS) {
			continue;
		}

		// Cloudflare returns a json array of one element containing the
		// actual result object
		json::object result;
		{
			json::array results;
			if (json["result"].get(results) != json::SUCCESS) {
				continue;
			}
			if (results.at(0).get(result) != json::SUCCESS) {
				continue;
			}
		}

		if (result["id"].get(zone_id_sv) != json::SUCCESS) {
			continue;
		}

		if (zone_id_sv.length() != DDNS_ZONE_ID_LENGTH) {
			continue;
		}

		found = true;
	}

	curl_easy_cleanup(curl);

	if (!found) {
		return DDNS_ERROR_GENERIC;
	}

	std::memcpy(zone_id, zone_id_sv.data(), zone_id_sv.length());
	zone_id[zone_id_sv.length()] = '\0';

	return DDNS_ERROR_OK;
}

DDNS_NODISCARD DDNS_PUB ddns_error ddns_get_zone_id_raw(
	const char* const DDNS_RESTRICT api_token,
	const char* const DDNS_RESTRICT zone_name,
	void**            DDNS_RESTRICT curl
) DDNS_NOEXCEPT {
	const std::size_t zone_name_length = std::strlen(zone_name);

	// -2 because this endpoint has a maximum record name length of 253
	constexpr std::size_t zone_name_max_length = DDNS_RECORD_NAME_MAX_LENGTH - 2U;

	if (std::strlen(api_token) != DDNS_API_TOKEN_LENGTH || zone_name_length > zone_name_max_length) {
		return DDNS_ERROR_USAGE;
	}

	constexpr std::string_view zones_url = "?per_page=1&name=";

	constexpr std::size_t request_url_capacity =
		base_url.length() +
		zones_url.length() +
		zone_name_max_length
	;

	// +1 because of '\0'
	char request_url[request_url_capacity + 1];

	// create the request url
	std::memcpy(request_url, base_url.data(), base_url.length());
	std::memcpy(request_url + base_url.length(), zones_url.data(), zones_url.length());
	std::memcpy(request_url + base_url.length() + zones_url.length(), zone_name, zone_name_length);
	request_url[base_url.length() + zones_url.length() + zone_name_length] = '\0';

	curl_slist* free_me_doh = curl_doh_setup(curl);
	curl_slist* free_me_headers = curl_auth_setup(curl, api_token);

	curl_get_setup(curl, request_url);

	const int curl_error = curl_easy_perform(*curl);

	curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, nullptr);
	curl_slist_free_all(free_me_headers);

	curl_easy_setopt(*curl, CURLOPT_RESOLVE, nullptr);
	curl_slist_free_all(free_me_doh);

	if (curl_error) {
		return DDNS_ERROR_GENERIC;
	}

	return DDNS_ERROR_OK;
}

DDNS_NODISCARD ddns_error ddns_get_record(
	const char* const DDNS_RESTRICT api_token,
	const char* const DDNS_RESTRICT zone_id,
	const char* const DDNS_RESTRICT record_name,
	const size_t record_ip_size, char* DDNS_RESTRICT record_ip,
	const size_t record_id_size, char* DDNS_RESTRICT record_id,
	bool* aaaa
) DDNS_NOEXCEPT {
	CURL* curl {curl_easy_init()};
	static_buffer response;

	curl_handle_setup(&curl, response);

	const ddns_error error = ddns_get_record_raw(api_token, zone_id, record_name, &curl);

	curl_easy_cleanup(curl);

	if (error) {
		return error;
	}

	json::parser parser;

	json::document json;
	if (parser.iterate(response.buffer, response.size, response.capacity).get(json) != json::SUCCESS) {
		return DDNS_ERROR_GENERIC;
	}

	// Cloudflare returns a json array of one element containing the
	// actual result object
	json::object result;
	{
		json::array results;
		if (json["result"].get(results) != json::SUCCESS) {
			return DDNS_ERROR_GENERIC;
		}
		if (results.at(0).get(result) != json::SUCCESS) {
			return DDNS_ERROR_GENERIC;
		}
	}

	std::string_view record_id_sv;
	if (result["id"].get(record_id_sv) != json::SUCCESS) {
		return DDNS_ERROR_GENERIC;
	}

	std::string_view type;
	if (result["type"].get(type) != json::SUCCESS) {
		return DDNS_ERROR_GENERIC;
	}

	std::string_view record_ip_sv;
	if (result["content"].get(record_ip_sv) != json::SUCCESS) {
		return DDNS_ERROR_GENERIC;
	}

	if (record_ip_sv.length() >= record_ip_size || record_id_sv.length() >= record_id_size) {
		return DDNS_ERROR_USAGE;
	}

	std::memcpy(record_ip, record_ip_sv.data(), record_ip_sv.length());
	record_ip[record_ip_sv.length()] = '\0';

	std::memcpy(record_id, record_id_sv.data(), record_id_sv.length());
	record_id[record_id_sv.length()] = '\0';

	*aaaa = (type == "AAAA");

	return DDNS_ERROR_OK;
}

DDNS_NODISCARD ddns_error ddns_get_record_raw(
	const char* DDNS_RESTRICT api_token,
	const char* DDNS_RESTRICT zone_id,
	const char* DDNS_RESTRICT record_name,
	void**      DDNS_RESTRICT curl
) DDNS_NOEXCEPT {
	const std::size_t record_name_length {std::strlen(record_name)};

	if (std::strlen(api_token) != DDNS_API_TOKEN_LENGTH || std::strlen(zone_id) != DDNS_ZONE_ID_LENGTH || record_name_length > DDNS_RECORD_NAME_MAX_LENGTH) {
		return DDNS_ERROR_USAGE;
	}

	constexpr std::string_view dns_records_url {"/dns_records?type=A,AAAA&name="};

	constexpr std::size_t request_url_capacity {
		base_url.length() +
		DDNS_ZONE_ID_LENGTH +
		dns_records_url.length() +
		DDNS_RECORD_NAME_MAX_LENGTH
	};

	const std::size_t request_url_length {
		base_url.length() +
		DDNS_ZONE_ID_LENGTH +
		dns_records_url.length() +
		record_name_length
	};

	// +1 because of '\0'
	char request_url[request_url_capacity + 1U];

	// Concatenate strings
	std::memcpy(request_url, base_url.data(), base_url.length());
	std::memcpy(request_url + base_url.length(), zone_id, DDNS_ZONE_ID_LENGTH);
	std::memcpy(request_url + base_url.length() + DDNS_ZONE_ID_LENGTH, dns_records_url.data(), dns_records_url.length());
	std::memcpy(request_url + base_url.length() + DDNS_ZONE_ID_LENGTH + dns_records_url.length(), record_name, record_name_length);
	request_url[request_url_length] = '\0';

	curl_slist* free_me_doh {curl_doh_setup(curl)};
	curl_slist* free_me_headers {curl_auth_setup(curl, api_token)};

	curl_get_setup(curl, request_url);

	const int curl_error = curl_easy_perform(*curl);

	curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, nullptr);
	curl_slist_free_all(free_me_headers);

	curl_easy_setopt(*curl, CURLOPT_RESOLVE, nullptr);
	curl_slist_free_all(free_me_doh);

	if (curl_error) {
		return DDNS_ERROR_GENERIC;
	}

	return DDNS_ERROR_OK;
}

DDNS_NODISCARD ddns_error ddns_update_record(
	const char* DDNS_RESTRICT api_token,
	const char* DDNS_RESTRICT zone_id,
	const char* DDNS_RESTRICT record_id,
	const char* DDNS_RESTRICT new_ip,
	const size_t record_ip_size, char* DDNS_RESTRICT record_ip
) DDNS_NOEXCEPT {
	CURL* curl {curl_easy_init()};
	static_buffer response;

	curl_handle_setup(&curl, response);

	const ddns_error error = ddns_update_record_raw(api_token, zone_id, record_id, new_ip, &curl);

	curl_easy_cleanup(curl);

	if (error) {
		return error;
	}

	json::parser parser;

	json::document json;
	if (parser.iterate(response.buffer, response.size, response.capacity).get(json) != json::SUCCESS) {
		return DDNS_ERROR_GENERIC;
	}

	json::object result;
	if (json["result"].get(result) != json::SUCCESS) {
		return DDNS_ERROR_GENERIC;
	}

	std::string_view record_ip_sv;
	if (result["content"].get(record_ip_sv) != json::SUCCESS) {
		return DDNS_ERROR_GENERIC;
	}

	if (record_ip_sv.length() >= record_ip_size) {
		return DDNS_ERROR_USAGE;
	}

	std::memcpy(record_ip, record_ip_sv.data(), record_ip_sv.length());
	record_ip[record_ip_sv.length()] = '\0';

	return DDNS_ERROR_OK;
}

DDNS_NODISCARD ddns_error ddns_update_record_raw(
	const char* DDNS_RESTRICT api_token,
	const char* DDNS_RESTRICT zone_id,
	const char* DDNS_RESTRICT record_id,
	const char* DDNS_RESTRICT new_ip,
	void**      DDNS_RESTRICT curl
) DDNS_NOEXCEPT {
	const std::size_t new_ip_length {std::strlen(new_ip)};

	if (std::strlen(api_token) != DDNS_API_TOKEN_LENGTH || std::strlen(zone_id) != DDNS_ZONE_ID_LENGTH || std::strlen(record_id) != DDNS_RECORD_ID_LENGTH || new_ip_length > DDNS_IP_ADDRESS_MAX_LENGTH) {
		return DDNS_ERROR_USAGE;
	}

	curl_slist* free_me_doh {curl_doh_setup(curl)};
	curl_slist* free_me_headers {curl_auth_setup(curl, api_token)};

	constexpr std::string_view dns_records_url {"/dns_records/"};

	constexpr std::size_t request_url_length {
		base_url.length() +
		DDNS_ZONE_ID_LENGTH +
		dns_records_url.length() +
		DDNS_RECORD_ID_LENGTH
	};

	// +1 because of '\0'
	char request_url[request_url_length + 1U];

	// Concatenate the strings to make the request url
	std::memcpy(request_url, base_url.data(), base_url.length());
	std::memcpy(request_url + base_url.length(), zone_id, DDNS_ZONE_ID_LENGTH);
	std::memcpy(request_url + base_url.length() + DDNS_ZONE_ID_LENGTH, dns_records_url.data(), dns_records_url.length());
	std::memcpy(request_url + base_url.length() + DDNS_ZONE_ID_LENGTH + dns_records_url.length(), record_id, DDNS_RECORD_ID_LENGTH);
	request_url[request_url_length] = '\0';

	constexpr std::string_view request_body_start {R"({"content": ")"};
	constexpr std::string_view request_body_end {"\"}"};

	constexpr std::size_t request_body_capacity {
		request_body_start.length() +
		DDNS_IP_ADDRESS_MAX_LENGTH +
		request_body_end.length()
	};

	const std::size_t request_body_length {
		request_body_start.length() +
		new_ip_length +
		request_body_end.length()
	};

	// This request buffer needs to be valid when calling curl_easy_perform()
	char request_body [request_body_capacity + 1];

	// Concatenate the strings to make the request body
	std::memcpy(request_body, request_body_start.data(), request_body_start.length());
	std::memcpy(request_body + request_body_start.length(), new_ip, new_ip_length);
	std::memcpy(request_body + request_body_start.length() + new_ip_length, request_body_end.data(), request_body_end.length());
	request_body[request_body_length] = '\0';

	curl_patch_setup(
		curl,
		request_url,
		request_body
	);

	const int curl_error {curl_easy_perform(*curl)};

	curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, nullptr);
	curl_slist_free_all(free_me_headers);

	curl_easy_setopt(*curl, CURLOPT_RESOLVE, nullptr);
	curl_slist_free_all(free_me_doh);

	if (curl_error) {
		return DDNS_ERROR_GENERIC;
	}

	return DDNS_ERROR_OK;
}

} // extern "C"
