/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "common.hpp"
#ifdef TACHI_HAS_GETADDRINFO
	#include <netdb.h>
	#include <arpa/inet.h>
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
		addrinfo* dns_response {nullptr};

		int error {
			getaddrinfo(test_record_name.data(), nullptr, nullptr, &dns_response)
		};
		if (error != 0) {
			std::cerr << "getaddrinfo: " << gai_strerror(error);
			std::exit(EXIT_FAILURE);
		}

		// TODO(tachi): handle IPv6
		std::string address {
			inet_ntoa(reinterpret_cast<sockaddr_in*>(dns_response->ai_addr)->sin_addr)
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
