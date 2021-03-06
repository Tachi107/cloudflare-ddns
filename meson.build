project(
	'cloudflare-ddns',
	'cpp',
	default_options: [
		'cpp_std=c++17',
		'buildtype=release',
		'b_ndebug=if-release',
		'b_lto=true'
	],
	version: '0.3.0',
	license: 'AGPL-3.0-or-later',
	meson_version: '>=0.46.0'
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
		dependency('threads'),
		dependency('yaml-cpp')
	],
	cpp_args: '-DOPENSSL_NO_SSL3_METHOD',
	install: true
)

if host_machine.system() == 'linux' or host_machine.system() == 'openbsd'
	install_data('config.yaml', install_dir: '/etc/cloudflare-ddns')
elif host_machine.system() == 'freebsd'
	install_data('config.yaml', install_dir: '/usr/local/etc/cloudflare-ddns')
else
	install_data('config.yaml')
endif
