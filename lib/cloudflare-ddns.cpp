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

/*
 * By reading Cloudflare's docs, I can see what is the maximum allowed
 * length of every parameter. With this knowledge, I can statically
 * figure out how much memory I need to make my requests, and I can
 * thus avoid dynamic memory allocations when creating the request URLs
 *
 * Ok, questo va bene per determinare la dimensione massima del buffer,
 * ma mi serve anche la dimensione effettiva, dato che devo inserire il
 * carattere terminatore e anche dire a memcpy il numero esatto di byte
 * che deve copiare. Per quanto riguarda la lunghezza di zone_id e
 * record_id non devo fare nulla di particolare, in quanto quella è sia
 * la lunghezza massima che la lunghezza effettiva (almeno, spero). Le
 * cose però cambiano quando si parla di record_name_length e
 * ip_address_length, perché la loro lunghezza effettiva è solitamente
 * minore rispetto a quella massima. Devo anche ricordarmi di fare dei
 * controlli per assicurarmi che la lunghezza del record_name e dell'ip
 * address inserito dall'utente siano minori di quelle massime, perché
 * altrimenti rischierei di incorrere in alcuni buffer overflow.
 */

extern "C" {

constexpr std::string_view base_url          {"https://api.cloudflare.com/client/v4/zones/"};
constexpr std::size_t zone_id_length         {32U};
constexpr std::size_t record_id_length       {32U};
constexpr std::size_t record_name_max_length {255U};
constexpr std::size_t ip_address_max_length  {46U};

namespace priv {

struct static_buffer {
	static constexpr std::size_t capacity {CURL_MAX_WRITE_SIZE};
	std::size_t size {0};
	char buffer[capacity];
};

static std::size_t write_data(
	char* TACHI_RESTRICT incoming_buffer,
	const std::size_t /*size*/, // size will always be 1
	const std::size_t count,
	static_buffer* TACHI_RESTRICT data
) TACHI_NOEXCEPT {
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

static void curl_handle_setup(
	CURL** TACHI_RESTRICT curl,
	const static_buffer& response_buffer
) TACHI_NOEXCEPT {
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

static void curl_doh_setup(CURL** TACHI_RESTRICT curl) TACHI_NOEXCEPT {
	struct curl_slist* manual_doh_address {nullptr};
	manual_doh_address = curl_slist_append(manual_doh_address, "cloudflare-dns.com:443:104.16.248.249,104.16.249.249,2606:4700::6810:f8f9,2606:4700::6810:f9f9");
	curl_easy_setopt(*curl, CURLOPT_RESOLVE, manual_doh_address);
	curl_easy_setopt(*curl, CURLOPT_DOH_URL, "https://cloudflare-dns.com/dns-query");
}

static void curl_auth_setup(CURL** TACHI_RESTRICT curl, const char* TACHI_RESTRICT const api_token) TACHI_NOEXCEPT {
	curl_easy_setopt(*curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
	curl_easy_setopt(*curl, CURLOPT_XOAUTH2_BEARER, api_token);
	struct curl_slist* headers {nullptr};
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, headers);
}

static void curl_get_setup(CURL** TACHI_RESTRICT curl, const char* TACHI_RESTRICT const url) TACHI_NOEXCEPT {
	curl_easy_setopt(*curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(*curl, CURLOPT_URL, url);
}

static void curl_patch_setup(CURL** TACHI_RESTRICT curl, const char* TACHI_RESTRICT const url, const char* TACHI_RESTRICT const body) TACHI_NOEXCEPT {
	curl_easy_setopt(*curl, CURLOPT_CUSTOMREQUEST, "PATCH");
	curl_easy_setopt(*curl, CURLOPT_URL, url);
	curl_easy_setopt(*curl, CURLOPT_POSTFIELDS, body);
}

} // namespace priv

int tachi_get_local_ip(
	size_t ip_size,
	char* TACHI_RESTRICT ip
) TACHI_NOEXCEPT {
	// Creating the handle and the response buffer
	CURL* curl {curl_easy_init()};
	priv::static_buffer response;

	priv::curl_handle_setup(&curl, response);
	priv::curl_doh_setup(&curl);
	priv::curl_get_setup(&curl, "https://one.one.one.one/cdn-cgi/trace");

	// Performing the request
	if (curl_easy_perform(curl) != 0) {
		return 1;
	}

	// Cleaning up the handle as I won't reuse it
	curl_easy_cleanup(curl);

	const std::string_view response_sv {response.buffer, response.size};
	// Parsing the response
	const std::size_t ip_begin {response_sv.find("ip=") + 3U};  // + 3 because "ip=" is 3 chars
	const std::size_t ip_end {response_sv.find('\n', ip_begin)};
	const std::size_t ip_length {ip_end - ip_begin};

	if (ip_length >= ip_size) {
		return 2;
	}

	// Copying the ip in the caller's buffer
	// Using memcpy because I don't need to copy the whole response
	std::memcpy(ip, response.buffer + ip_begin, ip_length);
	ip[ip_length] = '\0';

	return 0;
}

int tachi_get_record(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_name,
	size_t record_ip_size, char* TACHI_RESTRICT record_ip,
	size_t record_id_size, char* TACHI_RESTRICT record_id
) TACHI_NOEXCEPT {
	CURL* curl {curl_easy_init()};
	priv::static_buffer response;

	priv::curl_handle_setup(&curl, response);

	tachi_get_record_raw(api_token, zone_id, record_name, &curl);

	curl_easy_cleanup(curl);

	simdjson::dom::parser parser;

	simdjson::dom::element parsed;
	if (parser.parse(std::string_view{response.buffer, response.size}).get(parsed) != simdjson::SUCCESS) {
		return 1;
	}

	// Cloudflare returns a json array of one element containing the
	// actual result object
	simdjson::dom::element result;
	{
		simdjson::dom::array results;
		if (parsed["result"].get(results) != simdjson::SUCCESS) {
			return 1;
		}
		if ((*results.begin()).get(result) != simdjson::SUCCESS) {
			return 1;
		}
	}

	std::string_view record_ip_sv;
	if (result["content"].get(record_ip_sv) != simdjson::SUCCESS) {
		return 1;
	}

	std::string_view record_id_sv;
	if (result["id"].get(record_id_sv) != simdjson::SUCCESS) {
		return 1;
	}

	if (record_ip_sv.length() >= record_ip_size || record_id_sv.length() >= record_id_size) {
		return 2;
	}

	std::memcpy(record_ip, record_ip_sv.data(), record_ip_sv.length());
	record_ip[record_ip_sv.length()] = '\0';

	std::memcpy(record_id, record_id_sv.data(), record_id_sv.length());
	record_id[record_id_sv.length()] = '\0';

	return 0;
}

int tachi_get_record_raw(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_name,
	void**      TACHI_RESTRICT curl
) TACHI_NOEXCEPT {
	const std::size_t record_name_length {std::strlen(record_name)};

	// The length of API IDs should always be 32
	if (std::strlen(zone_id) != zone_id_length || record_name_length > record_name_max_length) {
		return 2;
	}

	priv::curl_doh_setup(curl);
	priv::curl_auth_setup(curl, api_token);

	constexpr std::string_view dns_records_url {"/dns_records?type=A,AAAA&name="};

	constexpr std::size_t request_url_capacity {
		base_url.length() +
		zone_id_length +
		dns_records_url.length() +
		record_name_max_length
	};

	const std::size_t request_url_length {
		base_url.length() +
		zone_id_length +
		dns_records_url.length() +
		record_name_length
	};

	// +1 because of '\0'
	char request_url[request_url_capacity + 1U];

	// Concatenate strings
	std::memcpy(request_url, base_url.data(), base_url.length());
	std::memcpy(request_url + base_url.length(), zone_id, zone_id_length);
	std::memcpy(request_url + base_url.length() + zone_id_length, dns_records_url.data(), dns_records_url.length());
	std::memcpy(request_url + base_url.length() + zone_id_length + dns_records_url.length(), record_name, record_name_length);
	request_url[request_url_length] = '\0';

	priv::curl_get_setup(curl, request_url);

	return curl_easy_perform(*curl);
}

int tachi_update_record(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_id,
	const char* TACHI_RESTRICT new_ip,
	size_t record_ip_size, char* TACHI_RESTRICT record_ip
) TACHI_NOEXCEPT {
	CURL* curl {curl_easy_init()};
	priv::static_buffer response;

	priv::curl_handle_setup(&curl, response);

	tachi_update_record_raw(api_token, zone_id, record_id, new_ip, &curl);

	curl_easy_cleanup(curl);

	simdjson::dom::parser parser;

	simdjson::dom::element parsed;
	if (parser.parse(std::string_view{response.buffer, response.size}).get(parsed) != simdjson::SUCCESS) {
		return 1;
	}

	simdjson::dom::element result;
	if (parsed["result"].get(result) != simdjson::SUCCESS) {
		return 1;
	}

	std::string_view record_ip_sv;
	if (result["content"].get(record_ip_sv) != simdjson::SUCCESS) {
		return 1;
	}

	if (record_ip_sv.length() >= record_ip_size) {
		return 2;
	}

	std::memcpy(record_ip, record_ip_sv.data(), record_ip_sv.length());
	record_ip[record_ip_sv.length()] = '\0';

	return 0;
}

int tachi_update_record_raw(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_id,
	const char* TACHI_RESTRICT new_ip,
	void**      TACHI_RESTRICT curl
) TACHI_NOEXCEPT {
	const std::size_t new_ip_length {std::strlen(new_ip)};

	// The length of API IDs should always be 32
	if (std::strlen(zone_id) != zone_id_length || std::strlen(record_id) != record_id_length || new_ip_length > ip_address_max_length) {
		return 2;
	}

	priv::curl_doh_setup(curl);
	priv::curl_auth_setup(curl, api_token);

	constexpr std::string_view dns_records_url {"/dns_records/"};

	constexpr std::size_t request_url_length {
		base_url.length() +
		zone_id_length +
		dns_records_url.length() +
		record_id_length
	};

	// +1 because of '\0'
	char request_url[request_url_length + 1U];

	// Concatenate the strings to make the request url
	std::memcpy(request_url, base_url.data(), base_url.length());
	std::memcpy(request_url + base_url.length(), zone_id, zone_id_length);
	std::memcpy(request_url + base_url.length() + zone_id_length, dns_records_url.data(), dns_records_url.length());
	std::memcpy(request_url + base_url.length() + zone_id_length + dns_records_url.length(), record_id, record_id_length);
	request_url[request_url_length] = '\0';

	constexpr std::string_view request_body_start {R"({"content": ")"};
	constexpr std::string_view request_body_end {"\"}"};

	constexpr std::size_t request_body_capacity {
		request_body_start.length() +
		ip_address_max_length +
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

	priv::curl_patch_setup(
		curl,
		request_url,
		request_body
	);

	return curl_easy_perform(*curl);
}

} // extern "C"
