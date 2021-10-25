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
test_args = []
if compiler.get_argument_syntax() == 'gcc'
	test_args += '-std=c++20'
elif compiler.get_argument_syntax() == 'msvc'
	test_args += '/std:c++20'
endif

test(
	'main',
	executable(
		'test_main',
		'test_main.cpp',
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
