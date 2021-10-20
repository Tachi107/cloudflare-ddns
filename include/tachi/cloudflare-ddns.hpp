/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#include <string>
#include <curl/curl.h>

#if defined _WIN32 || defined __CYGWIN__
	#ifdef TACHI_BUILD_SHARED
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


/*
 * There are two kinds of functions; the one that is self contained, thread
 * safe, that create and use their own cURL handle, and the other one that
 * borrows mutably a cURL handle, a more efficient approach but that is not
 * thread safe. They require that the handle is already properly
 * initialized as they use curl_easy_setopt just to set up the request type
 * and URL.
 *
 * The user of the library is responsable for calling curl_global_init.
 */

namespace tachi {

TACHI_PUB std::string get_local_ip();

TACHI_PUB std::string get_record_ip_raw(std::string_view api_token, std::string_view zone_id, std::string_view record_name);

namespace priv {

TACHI_PRIV std::size_t write_data(char* incoming_buffer, std::size_t size, std::size_t count, std::string* data);

TACHI_PRIV void curl_handle_setup(CURL** handle, const std::string& response_buffer) noexcept;

TACHI_PRIV void curl_doh_setup(CURL** handle) noexcept;

} // namespace priv

} // namespace tachi
