project(
	'cloudflare-ddns',
	'cpp',
	default_options: [
		'cpp_std=c++17',
		'b_ndebug=if-release',
		'b_lto=true'
	],
	version: '0.0.1',
	license: 'AGPL-3.0-or-later'
)

executable(
	'cloudflare-ddns',
	sources: 'cloudflare-ddns/main.cpp',
	dependencies: [
		subproject('cpr').get_variable('cpr_dep'),
		dependency('nlohmann_json'),
		dependency('simdjson')
	],
	cpp_args: '-DOPENSSL_NO_SSL3_METHOD'
)
