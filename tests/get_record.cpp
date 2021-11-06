/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "common.hpp"
#ifdef TACHI_HAS_GETADDRINFO
	#if __has_include(<netdb.h>)
		#include <netdb.h>
	#endif
	#if __has_include(<arpa/inet.h>)
		#include <arpa/inet.h>
	#endif
	#if __has_include(<ws2tcpip.h>)
		#include <ws2tcpip.h>
	#endif
#else
	#error "Test not implemented on this platform"
#endif
#include <array>

int main() {
		/**
	 * Get the current IP of an A record and compare if against the
	 * return value of tachi::get_record()
	 */
	"get_record"_test = [] {
		#if TACHI_HAS_GETADDRINFO
		#if __has_include(<ws2tcpip.h>)
			WSADATA wsaData;
			// Manually setting the version to 2.2 instead of using MAKEWORD
			WSAStartup(0b00000010'00000010, &wsaData);
		#endif
		addrinfo* dns_response {nullptr};

		const int error {
			getaddrinfo(test_record_name.data(), nullptr, nullptr, &dns_response)
		};
		expect(eq(error, 0)) << "getaddrinfo: " << gai_strerror(error);

		std::array<char, INET6_ADDRSTRLEN> address;
		inet_ntop(
			dns_response->ai_addr->sa_family,
			// I don't know why I have to add +2, I just know that
			// the first two chars in sa_data are empty
			dns_response->ai_addr->sa_data + 2,
			address.data(), address.size()
		);

		freeaddrinfo(dns_response);
		#endif

		std::array<char, TACHI_IP_ADDRESS_MAX_LENGTH> record_ip;
		std::array<char, TACHI_RECORD_ID_LENGTH + 1> record_id;

		expect(eq(tachi_get_record(
			test_api_token.data(),
			test_zone_id.data(),
			test_record_name.data(),
			record_ip.size(), record_ip.data(),
			record_id.size(), record_id.data()
		), 0));

		expect(eq(
			std::string_view{address.data()},
			std::string_view{record_ip.data()}
		));
	};
}
