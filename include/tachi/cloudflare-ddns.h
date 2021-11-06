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
 *
 * I'm following the C23 conventions for function parameter order, see
 * https://wg14.link/n2086
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

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

#if defined(__cplusplus)
	#if __cplusplus >= 201103L
		#define TACHI_NOEXCEPT noexcept
	#else
		#define TACHI_NOEXCEPT throw()
	#endif
#else
	#if __GNUC__ >= 4
		#define TACHI_NOEXCEPT __attribute__ ((nothrow))
	#else
		#define TACHI_NOEXCEPT
	#endif
#endif

#if __STDC_VERSION__ >= 199901L
	#define TACHI_RESTRICT   restrict
#elif defined(__GNUC__)
	#define TACHI_RESTRICT __restrict__
#elif defined(_MSC_VER)
	#define TACHI_RESTRICT __restrict
#else
	#define TACHI_RESTRICT
#endif

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
TACHI_PUB int tachi_get_local_ip(
	size_t ip_size, char* TACHI_RESTRICT ip
) TACHI_NOEXCEPT;

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
 * get_record_raw(), but you'll have to parse the result yourself.
 */
TACHI_PUB int tachi_get_record(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_name,
	size_t record_ip_size, char* TACHI_RESTRICT record_ip,
	size_t record_id_size, char* TACHI_RESTRICT record_id
) TACHI_NOEXCEPT;

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
TACHI_PUB int tachi_get_record_raw(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_name,
	void**      TACHI_RESTRICT curl
) TACHI_NOEXCEPT;

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
TACHI_PUB int tachi_update_record(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_id,
	const char* TACHI_RESTRICT new_ip,
	size_t record_ip_size, char* TACHI_RESTRICT record_ip
) TACHI_NOEXCEPT;

/**
 * Update the IP address of a given A/AAAA DNS record
 * This function sends a PATCH request to Cloudflare's API and sets the IP
 * address of a given A/AAAA record to the one passed as argument to the fn.
 * You must pass in your own cURL handle, which allows for better
 * performance but has greater complexity. If you don't particurarly care
 * about performance you can use the simpler update_record() function.
 */
TACHI_PUB int tachi_update_record_raw(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_id,
	const char* TACHI_RESTRICT new_ip,
	void**      TACHI_RESTRICT curl
) TACHI_NOEXCEPT;

#ifdef __cplusplus
} // extern "C"
#endif
