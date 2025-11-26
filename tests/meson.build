# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
# SPDX-FileCopyrightText: 2025 Toni Melisma
#
# SPDX-License-Identifier: AGPL-3.0-or-later

credentials_hpp = configure_file(
	input: 'credentials.hpp.in',
	output: 'credentials.hpp',
	configuration: {
		'test_api_token':     get_option('test_api_token'),
		'test_zone_id':       get_option('test_zone_id'),
		'test_record_name':   get_option('test_record_name'),
		'test_record_name_2': get_option('test_record_name_2')
	}
)

boost_ut_dep = dependency('boost.ut', fallback: ['ut', 'boostut_dep'])

test_args = []
test_deps = []
test_opts = ['b_ndebug=false']

if compiler.get_argument_syntax() == 'msvc'
	test_opts += 'cpp_std=c++latest'
elif meson.version().version_compare('>=0.57.0')
	test_opts += 'cpp_std=c++20'
else
	test_opts += 'cpp_std=c++2a'
endif

if compiler.has_function('getaddrinfo')
	test_args += '-DDDNS_HAS_GETADDRINFO'
endif

if compiler.has_header('ws2tcpip.h')
	test_deps += compiler.find_library('Ws2_32')
	if compiler.has_function('getaddrinfo', prefix: '#include <ws2tcpip.h>', dependencies: test_deps)
		test_args += '-DDDNS_HAS_GETADDRINFO'
	endif
endif

tests = [
	'get_local_ip',
	'get_record',
	'search_zone_id',
	'update_record',
	'test_parsing',
	'update_multiple_records'
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
			override_options: test_opts,
			sources: credentials_hpp
		)
	)
endforeach
