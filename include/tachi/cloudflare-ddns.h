/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
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
 * http://www.open-std.org/jtc1/sc22/wg14/www/docs/n2611.htm
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define TACHI_ZONE_ID_LENGTH         32U
#define TACHI_RECORD_ID_LENGTH       32U
#define TACHI_RECORD_NAME_MAX_LENGTH 255U
#define TACHI_IP_ADDRESS_MAX_LENGTH  46U

#include <stddef.h> /* size_t */
#include <stdbool.h> /* bool, true */

/*
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
#	ifdef TACHI_SHARED_LIB
#		ifdef TACHI_BUILDING_DLL
#			ifdef __GNUC__
#				define TACHI_PUB __attribute__ ((dllexport))
#			else
#				define TACHI_PUB __declspec(dllexport)
#			endif
#		else
#			ifdef __GNUC__
#				define TACHI_PUB __attribute__ ((dllimport))
#			else
#				define TACHI_PUB __declspec(dllimport)
#			endif
#		endif
#	else
#		define TACHI_PUB
#	endif
#	define TACHI_PRIV
#else
#	if defined(__GNUC__) && __GNUC__ >= 4
#		define TACHI_PUB __attribute__ ((visibility ("default")))
#		define TACHI_PRIV __attribute__ ((visibility ("hidden")))
#	else
#		define TACHI_PUB
#		define TACHI_PRIV
#	endif
#endif

#if defined(__cplusplus)
#	if __cplusplus >= 201103L
#		define TACHI_NOEXCEPT noexcept
#	else
#		define TACHI_NOEXCEPT throw()
#	endif
#else
#	if defined(__GNUC__) && __GNUC__ >= 4
#		define TACHI_NOEXCEPT __attribute__ ((nothrow))
#	else
#		define TACHI_NOEXCEPT
#	endif
#endif

#if !defined(__cplusplus) && (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#	define TACHI_RESTRICT   restrict
#elif defined(__GNUC__)
#	define TACHI_RESTRICT __restrict__
#elif defined(_MSC_VER)
#	define TACHI_RESTRICT __restrict
#else
#	define TACHI_RESTRICT
#endif

#if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202300L)
#	define TACHI_NODISCARD [[nodiscard]]
#elif (defined(__cplusplus) && __cplusplus >= 201103L) && defined(__GNUC__)
#	define TACHI_NODISCARD [[gnu::warn_unused_result]]
#elif defined(__GNUC__)
#	define TACHI_NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#	define TACHI_NODISCARD _Check_return_
#else
#	define TACHI_NODISCARD
#endif

/**
 * Get the public IP address of the machine
 *
 * With this function you can get the current public IP address of your
 * machine, so that you can know if the DNS record needs to be updated.
 *
 * It creates its own cURL handle under the hood and writes the IP address
 * in dot-decimal notation in the ip parameter. If ip_size is too small,
 * the function returns 2; if some other error occurs, it returns 1. It
 * uses Cloudflare to determine the public address, querying
 * https://one.one.one.one/cdn-cgi/trace
 *
 * Depending on the value of the ipv6 parameter, the function will either
 * force the HTTP request to use IPv4 (false) or IPv6 (true), also
 * determining the kind of IP address returned.
 */
TACHI_NODISCARD TACHI_PUB int tachi_get_local_ip(
	size_t ip_size, char* TACHI_RESTRICT ip, bool ipv6
) TACHI_NOEXCEPT;

/**
 * Get the current IP address of a given A/AAAA DNS record
 *
 * This function queries Cloudflare's API to retrieve the status of a given
 * A or AAAA record, returning its current IP address, its internal ID that
 * can be used to refer to it in the API and a boolean indicating whether
 * it is an A or AAAA record. The three values are written in record_ip,
 * record_id, and aaaa respectively. If their _size is too small,
 * the function returns 2; if some other error occurs, it returns 1. Since
 * Cloudflare's API returns data in JSON form, the function uses simdjson
 * to parse that data as fast as possible. Since the function has to access
 * the response of the HTTP GET request it has to create and manage its own
 * cURL handle internally, and this hurts a bit in terms of performance. If
 * you prefer to control your own cURL handles to get better performance you
 * can use get_record_raw(), but you'll have to parse the result yourself.
 */
TACHI_NODISCARD TACHI_PUB int tachi_get_record(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_name,
	size_t record_ip_size, char* TACHI_RESTRICT record_ip,
	size_t record_id_size, char* TACHI_RESTRICT record_id,
	bool* aaaa
) TACHI_NOEXCEPT;

/**
 * Query the API for the status of a given A/AAAA DNS record
 *
 * This function queries Cloudflare's API to retrieve the status of a given
 * A or AAAA record, returning the raw JSON response, and allows you to
 * pass in a preexisting cURL handle, to reuse connections and improve
 * performance. This also allows greater flexibility compared to
 * get_record(), but you'll have to consult Cloudflare's API reference and
 * you'll also need to parse the result yourself. If the length of the zone
 * id is not TACHI_ZONE_ID_LENGTH, or if the length of the record name is
 * greater than TACHI_RECORD_NAME_MAX_LENGTH, the function returns 2; if
 * something goes wrong with the HTTP request, it returns the value of
 * curl_easy_perform().
 */
TACHI_NODISCARD TACHI_PUB int tachi_get_record_raw(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_name,
	void**      TACHI_RESTRICT curl
) TACHI_NOEXCEPT;

/**
 * Update the IP address of a given A/AAAA DNS record
 *
 * This function sends a PATCH request to Cloudflare's API and sets the IP
 * address of a given A/AAAA record, identified by record_id, to the one
 * passed as new_ip. It manages its own cURL handle under the hood, and
 * writes the IP that Cloudflare received and set in record_ip, if
 * successful, parsed using simdjson. If the size of the out parameter is
 * smaller than the size of the IP set by Cloudflare, or if the length of
 * the zone id is not TACHI_ZONE_ID_LENGTH, or if the length of the record
 * name is greater than TACHI_RECORD_NAME_MAX_LENGTH, the function returns
 * 2; on any other error, it returns 1. This function is thread safe, but
 * it creates and destroys a cURL handle by  itself, which may be slow. If
 * you want faster performance by reusing the same handle you can look into
 * update_record_raw().
 */
TACHI_NODISCARD TACHI_PUB int tachi_update_record(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_id,
	const char* TACHI_RESTRICT new_ip,
	size_t record_ip_size, char* TACHI_RESTRICT record_ip
) TACHI_NOEXCEPT;

/**
 * Update the IP address of a given A/AAAA DNS record
 *
 * This function sends a PATCH request to Cloudflare's API and sets the IP
 * address of a given A/AAAA record, identified by record_id, to the one
 * passed as new_ip. You must pass in your own cURL handle, which allows
 * for better performance but has greater complexity. If the length of the
 * zone id is not TACHI_ZONE_ID_LENGTH, or if the length of the record name
 * is greater than TACHI_RECORD_NAME_MAX_LENGTH, the function returns 2; if
 * something goes wrong with the HTTP request, it returns the value of
 * curl_easy_perform(). If you don't particularly care about performance
 * you can use the simpler update_record() function.
 */
TACHI_NODISCARD TACHI_PUB int tachi_update_record_raw(
	const char* TACHI_RESTRICT api_token,
	const char* TACHI_RESTRICT zone_id,
	const char* TACHI_RESTRICT record_id,
	const char* TACHI_RESTRICT new_ip,
	void**      TACHI_RESTRICT curl
) TACHI_NOEXCEPT;

#ifdef __cplusplus
} /* extern "C" */
#endif
