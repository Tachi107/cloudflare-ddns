project(
	'cloudflare-ddns',
	'cpp',
	default_options: [
		'cpp_std=c++17',
		'buildtype=release',
		'b_ndebug=if-release',
		'b_lto=true'
	],
	version: '0.2.0',
	license: 'AGPL-3.0-or-later'
)

simdjson_dep = dependency('simdjson', required: false)
if not simdjson_dep.found()
	cmake = import('cmake')
	simdjson_options = cmake.subproject_options()
	simdjson_options.add_cmake_defines({'SIMDJSON_JUST_LIBRARY': true})
	cmake.subproject('simdjson', options: simdjson_options).dependency('simdjson')
endif

executable(
	'cloudflare-ddns',
	sources: 'cloudflare-ddns/main.cpp',
	dependencies: [
		dependency('libcurl'),
		simdjson_dep,
		dependency('threads')
	],
	cpp_args: '-DOPENSSL_NO_SSL3_METHOD',
	install: true
)
