project(
	'cloudflare-ddns',
	'cpp',
	default_options: [
		'cpp_std=c++17',
		'buildtype=release',
		'b_ndebug=if-release',
		'b_lto=true'
	],
	version: '0.1.0',
	license: 'AGPL-3.0-or-later'
)

executable(
	'cloudflare-ddns',
	sources: 'cloudflare-ddns/main.cpp',
	dependencies: [
		dependency('libcurl'),
		dependency('simdjson'),
		dependency('threads')
	],
	cpp_args: '-DOPENSSL_NO_SSL3_METHOD',
	install: true
)
