# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
#
# SPDX-License-Identifier: AGPL-3.0-or-later

project(
	'cloudflare-ddns',
	'cpp',
	default_options: [
		'cpp_std=c++17',
		'buildtype=release',
		'warning_level=3',
		'b_ndebug=if-release',
		'b_lto=true'
	],
	version: '0.4.0',
	license: 'AGPL-3.0-or-later',
	meson_version: '>=0.53.0'
)

compiler = meson.get_compiler('cpp')

windows_deps = []

cmake = import('cmake')

libcurl_dep = dependency('libcurl', required: false)
if not libcurl_dep.found()
	if compiler.get_id() == 'clang' and compiler.get_linker_id() != 'ld.lld'
		error('When using Clang you must also use LLD. You can use it by setting the CXX_LD environment variable to lld')
	endif
	if meson.version().version_compare('>=0.55.0')
		libcurl_options = cmake.subproject_options()
		libcurl_options.add_cmake_defines({
			'CMAKE_BUILD_TYPE': get_option('buildtype'),
			'CMAKE_POSITION_INDEPENDENT_CODE': get_option('b_staticpic'),
			'MSVC_RUNTIME_LIBRARY': 'MultiThreaded',
			'BUILD_SHARED_LIBS': false,
			'BUILD_CURL_EXE': false,
			'CURL_LTO': true,
			'HTTP_ONLY': true,
			'CURL_DISABLE_COOKIES': true,
			'CURL_DISABLE_VERBOSE_STRINGS': true,
			'ENABLE_UNIX_SOCKETS': false
		})
		if host_machine.system() == 'windows'
			libcurl_options.add_cmake_defines({
				'CURL_STATIC_CRT': true,
				'CMAKE_USE_SCHANNEL': true,
				'CURL_TARGET_WINDOWS_VERSION': '0x0A00' # Win 10
			})
			windows_deps += compiler.find_library('crypt32')
			windows_deps += compiler.find_library('ws2_32')
		endif
		libcurl_options.set_install(false)
		libcurl_options.append_compile_args('c', '-DOPENSSL_NO_SSL3_METHOD', '-DCURL_STATICLIB')
		libcurl_dep = cmake.subproject('curl', options: libcurl_options).dependency('libcurl')
	else
		libcurl_dep = cmake.subproject('curl').dependency('libcurl')
	endif
endif

simdjson_dep = dependency('simdjson', required: false)
if not simdjson_dep.found()
	if compiler.get_id() == 'clang' and compiler.get_linker_id() != 'ld.lld'
		error('When using Clang you must also use LLD. You can use it by setting the CXX_LD environment variable to lld')
	endif
	if meson.version().version_compare('>=0.55.0')
		simdjson_options = cmake.subproject_options()
		simdjson_options.add_cmake_defines({
			'CMAKE_BUILD_TYPE': get_option('buildtype'),
			'CMAKE_POSITION_INDEPENDENT_CODE': get_option('b_staticpic'),
			'MSVC_RUNTIME_LIBRARY': 'MultiThreaded',
			'BUILD_SHARED_LIBS': false,
			'SIMDJSON_WINDOWS_DLL': false,
			'SIMDJSON_USING_WINDOWS_DYNAMIC_LIBRARY': false
		})
		simdjson_options.set_install(false)
		if compiler.get_argument_syntax() == 'msvc'
			simdjson_options.append_compile_args('c', '/MT')
			simdjson_options.append_compile_args('cpp', '/MT')
		endif
		simdjson_dep = cmake.subproject('simdjson', options: simdjson_options).dependency('simdjson')
	else
		simdjson_dep = cmake.subproject('simdjson').dependency('simdjson')
	endif
endif

tomlplusplus_dep = dependency(
	'tomlplusplus',
	fallback: ['tomlplusplus', 'tomlplusplus_dep'],
	default_options: ['default_library=static']
)

config_path_hpp = ''
subdir('src')

if get_option('tests')
	subdir('tests')
endif

libcloudflare_ddns = library(
	'cloudflare-ddns',
	'lib'/'cloudflare-ddns.cpp',
	cpp_args: '-DTACHI_BUILD_LIBRARY',
	dependencies: [
		libcurl_dep,
		simdjson_dep,
		tomlplusplus_dep,
		windows_deps
	],
	gnu_symbol_visibility: 'hidden',
	include_directories: 'include',
	install: true,
	version: meson.project_version()
)

cloudflare_ddns_dep = declare_dependency(link_with: libcloudflare_ddns, version: meson.project_version())

executable(
	'cloudflare-ddns',
	'src'/'main.cpp',
	dependencies: [
		cloudflare_ddns_dep,
		libcurl_dep,
		simdjson_dep,
		tomlplusplus_dep,
		dependency('threads'),
		windows_deps
	],
	gnu_symbol_visibility: 'hidden',
	include_directories: 'include',
	install: true,
	sources: config_path_hpp
)

install_data('config.toml', install_dir: get_option('sysconfdir')/'cloudflare-ddns')
