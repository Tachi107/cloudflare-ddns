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

namespace tachi {

TACHI_PUB std::string get_local_ip();


namespace priv {

// È usata una volta sola, potrei usare una lambda
// NOT thread-safe, writes into a buffer and uses an handle both owned elsewhere
TACHI_PRIV void get_request(CURL** curl_handle, std::string_view request_uri);

TACHI_PRIV std::size_t write_data(char* incoming_buffer, std::size_t size, std::size_t count, std::string* data);

} // namespace priv

} // namespace tachi
