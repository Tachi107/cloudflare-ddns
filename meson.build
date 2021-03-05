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
	if meson.version().version_compare('>=0.55.0')
		simdjson_options = cmake.subproject_options()
		simdjson_options.add_cmake_defines({'SIMDJSON_JUST_LIBRARY': true})
		simdjson_dep = cmake.subproject('simdjson', options: simdjson_options).dependency('simdjson')
	else
		simdjson_dep = cmake.subproject('simdjson').dependency('simdjson')
	endif
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
