/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * There are two kinds of functions; the one that is self contained, thread
 * safe, that creates and uses its own cURL handle, and the other one that
 * borrows mutably a cURL handle, a more efficient approach but that is not
 * thread safe. They require that the handle is already properly
 * initialized as they use curl_easy_setopt just to set up the request type
 * and URL.
 *
 * The user of the library is responsable for calling curl_global_init.
 */

#pragma once
#include <string>
#include <utility>

/**
 * When dealing with the shared library on Windows, a few things need
 * to happen, depending on the situation. When building the shared lib,
 * all public symbols need to be marked with dllexport, and when using
 * that shared library users need to import that symbols, and this is
 * accomplished by marking them with dllimport. So, TACHI_SHARED_LIB
 * needs to always be present when dealing with shared libraries,
 * while TACHI_BUILDING_DLL is only needed when building it.
 *
 * Thanks https://github.com/myd7349/Ongoing-Study/blob/master/cpp/CMake/libfoo_v2/include/libfoo/foo.h
 */
#if defined _WIN32 || defined __CYGWIN__
	#ifdef TACHI_SHARED_LIB
		#ifdef TACHI_BUILDING_DLL
			#ifdef __GNUC__
				#define TACHI_PUB __attribute__ ((dllexport))
			#else
				#define TACHI_PUB __declspec(dllexport)
			#endif
		#else
			#ifdef __GNUC__
				#define TACHI_PUB __attribute__ ((dllimport))
			#else
				#define TACHI_PUB __declspec(dllimport)
			#endif
		#endif
	#else
		#define TACHI_PUB
	#endif
	#define TACHI_PRIV
#else
	#if __GNUC__ >= 4
		#define TACHI_PUB __attribute__ ((visibility ("default")))
		#define TACHI_PRIV __attribute__ ((visibility ("hidden")))
	#else
		#define TACHI_PUB
		#define TACHI_PRIV
	#endif
#endif

namespace tachi {

/**
 * Get the public IP address of the machine
 *
 * With this function you can get the current public IP address of your
 * machine, so that you can know if the DNS record needs to be updated.
 *
 * It creates its own cURL handle under the hood and returns a string
 * containing the IP address in dot-decimal notation. It uses Cloudflare
 * to determine the public address, querying https://1.1.1.1/cdn-cgi/trace
 */
TACHI_PUB std::string get_local_ip();

/**
 * Get the current IP address of a given A/AAAA DNS record
 *
 * This function queries Cloudflare's API to retreive the status of a given
 * A or AAAA record, returning its current IP address and its internal ID,
 * that can be used to refer to it in the API. Since Cloudflare's API
 * returns data in JSON form, the function uses simdjson to parse that data
 * as fast as possible. Since the function has to access the response of
 * the HTTP GET request it has to create and manage its own cURL handle
 * internally, and this hurts a bit in terms of performance. If you prefer
 * to control your own cURL handles to get better performance you can use
 * the get_record_raw() version that takes borrows mutably a cURL handle,
 * but you'll have to parse the result yourself.
 */
TACHI_PUB std::pair<const std::string, const std::string> get_record(const std::string& api_token, const std::string& zone_id, const std::string& record_name);

/**
 * Query the API for the status of a given A/AAAA DNS record
 *
 * This function queries Cloudflare's API to retreive the status of a given
 * A or AAAA record, returning the raw JSON response. This allows greater
 * flexibility compared to get_record(), but you'll have to consult
 * Cloudflare's API reference and you'll also need to parse the result
 * yourself. This isn't any faster than get_record(), so if you're looking
 * for a way to improve performance you may want to use the get_record_raw
 * version that takes a cURL handle as an argument.
 */
TACHI_PUB std::string get_record_raw(const std::string& api_token, const std::string& zone_id, const std::string& record_name);

/**
 * Query the API for the status of a given A/AAAA DNS record
 *
 * This function queries Cloudflare's API to retreive the status of a given
 * A or AAAA record, returning the raw JSON response, and allows you to
 * pass in a preexistent cURL handle, to reuse connections and improve
 * performance. This also allows greater flexibility compared to
 * get_record(), but you'll have to consult Cloudflare's API reference and
 * you'll also need to parse the result yourself.
 */
TACHI_PUB void get_record_raw(const std::string& api_token, const std::string& zone_id, const std::string& record_name, void** curl);

/**
 * Update the IP address of a given A/AAAA DNS record
 *
 * This function sends a PATCH request to Cloudflare's API and sets the IP
 * address of a given A/AAAA record to the one passed as argument to the fn.
 * It manages its own cURL handle under the hood, and returns the IP that
 * Cloudflare received and set, if successfull, parsed using simdjson. This
 * function is thread safe, but it creates and destroys a cURL handle by
 * itself, which may be slow. If you want faster performance by reusing the
 * same handle you can look into update_record_raw().
 */
TACHI_PUB std::string update_record(const std::string &api_token, const std::string &zone_id, const std::string &record_id, const std::string& new_ip);

/**
 * Update the IP address of a given A/AAAA DNS record
 * This function sends a PATCH request to Cloudflare's API and sets the IP
 * address of a given A/AAAA record to the one passed as argument to the fn.
 * You must pass in your own cURL handle, which allows for better
 * performance but has greater complexity. If you don't particurarly care
 * about performance you can use the simpler update_record() function.
 */
TACHI_PUB void update_record_raw(const std::string &api_token, const std::string &zone_id, const std::string &record_id, const std::string& new_ip, void** curl);

namespace priv {

extern "C" {

TACHI_PRIV std::size_t write_data(char* incoming_buffer, std::size_t size, std::size_t count, std::string* data);

TACHI_PRIV void curl_handle_setup(void** curl, const std::string& response_buffer) noexcept;

TACHI_PRIV void curl_doh_setup(void** curl) noexcept;

TACHI_PRIV void curl_auth_setup(void** curl, const char* api_token) noexcept;

TACHI_PRIV void curl_get_setup(void** curl, const char* url) noexcept;

TACHI_PRIV void curl_patch_setup(void** curl, const char* url, const char* body) noexcept;

} // extern "C"

} // namespace priv

} // namespace tachi
