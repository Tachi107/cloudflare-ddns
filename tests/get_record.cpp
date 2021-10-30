/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "common.hpp"
#ifdef TACHI_HAS_GETADDRINFO
	#ifdef TACHI_HAS_NETDB_H
		#include <netdb.h>
	#endif
	#ifdef TACHI_HAS_ARPA_INET_H
		#include <arpa/inet.h>
	#endif
	#ifdef TACHI_HAS_WS2TCPIP_H
		#include <ws2tcpip.h>
	#endif
#else
	#error "Test not implemented on this platform"
#endif

int main() {
		/**
	 * Get the current IP of an A record and compare if against the
	 * return value of tachi::get_record()
	 */
	"get_record"_test = [] {
		#if TACHI_HAS_GETADDRINFO
		#if TACHI_HAS_WS2TCPIP_H
			WSADATA wsaData;
			// Manually setting the version to 2.2 instead of using MAKEWORD
			WSAStartup(0b00000010'00000010, &wsaData);
		#endif
		addrinfo* dns_response {nullptr};

		const int error {
			getaddrinfo(test_record_name.data(), nullptr, nullptr, &dns_response)
		};
		if (error != 0) {
			std::cerr << "getaddrinfo: " << gai_strerror(error);
			std::exit(EXIT_FAILURE);
		}

		std::array<char, INET6_ADDRSTRLEN> address_buf;
		const std::string address {
			inet_ntop(
				dns_response->ai_addr->sa_family,
				// I don't know why I have to add +2, I just know that
				// the first two chars in sa_data are empty
				dns_response->ai_addr->sa_data + 2,
				address_buf.data(), address_buf.size()
			)
		};

		freeaddrinfo(dns_response);
		#endif

		expect(eq(
			address,
			tachi::get_record(
				std::string{test_api_token},
				std::string{test_zone_id},
				std::string{test_record_name}
			).first
		));
	};
}
