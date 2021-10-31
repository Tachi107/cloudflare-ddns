# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
#
# SPDX-License-Identifier: AGPL-3.0-or-later

credentials_hpp = configure_file(
	input: 'credentials.hpp.in',
	output: 'credentials.hpp',
	configuration: {
		'test_api_token':   get_option('test_api_token'),
		'test_zone_id':     get_option('test_zone_id'),
		'test_record_name': get_option('test_record_name')
	}
)

boost_ut_dep = dependency('boost.ut', fallback: ['ut', 'boostut_dep'])

# Enable C++20 for tests (required by ut)
test_args = [compiler.get_supported_arguments('-std=c++20', '-std=c++2a', '/std:c++20')[0]]
test_deps = []

if compiler.has_function('getaddrinfo')
	test_args += '-DTACHI_HAS_GETADDRINFO'
endif

if compiler.has_header('netdb.h')
	test_args += '-DTACHI_HAS_NETDB_H'
endif

if compiler.has_header('arpa/inet.h')
	test_args += '-DTACHI_HAS_ARPA_INET_H'
endif

if compiler.has_header('ws2tcpip.h')
	test_args += '-DTACHI_HAS_WS2TCPIP_H'
	test_deps += compiler.find_library('Ws2_32')
	if compiler.has_function('getaddrinfo', prefix: '#include <ws2tcpip.h>', dependencies: test_deps)
		test_args += '-DTACHI_HAS_GETADDRINFO'
	endif
endif

tests = [
	'get_local_ip',
	'get_record',
	'update_record'
]

foreach test : tests
	test(
		test,
		executable(
			test,
			test + '.cpp',
			cpp_args: test_args,
			dependencies: [
				boost_ut_dep,
				cloudflare_ddns_dep,
				libcurl_dep
			],
			gnu_symbol_visibility: 'hidden',
			sources: credentials_hpp
		)
	)
endforeach
