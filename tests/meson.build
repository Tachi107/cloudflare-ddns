# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
#
# SPDX-License-Identifier: AGPL-3.0-or-later

configure_file(
	input: 'credentials.hpp.in',
	output: 'credentials.hpp',
	configuration: {
		'test_api_token':   get_option('test_api_token'),
		'test_zone_id':     get_option('test_zone_id'),
		'test_record_name': get_option('test_record_name')
	}
)

boost_ut_dep = dependency('boost.ut', fallback: ['ut', 'boostut_dep'])

test(
	'main',
	executable(
		'test_main',
		'test_main.cpp',
		dependencies: boost_ut_dep
	)
)
