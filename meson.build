# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
#
# SPDX-License-Identifier: AGPL-3.0-or-later

project(
	'cloudflare-ddns',
	'cpp',
	default_options: [
		'buildtype=release',
		'cpp_std=c++17',
		'warning_level=3',
		'b_lto=true',
		'b_ndebug=if-release',
		'cpp_rtti=false'
	],
	version: '2.0.1',
	license: 'AGPL-3.0-or-later OR LGPL-3.0-or-later',
	meson_version: '>=0.53.0'
)

compiler = meson.get_compiler('cpp')

extra_deps = []

libcurl_dep = dependency('libcurl', required: false)
if not libcurl_dep.found()
	if compiler.get_id() == 'clang' and compiler.get_linker_id() != 'ld.lld'
		warning('When using Clang you must also use LLD. You can use it by setting the CXX_LD environment variable to lld')
	endif
	cmake = import('cmake')
	if meson.version().version_compare('>=0.55.0')
		libcurl_options = cmake.subproject_options()
		libcurl_options.add_cmake_defines({
			'CMAKE_BUILD_TYPE': get_option('buildtype'),
			'CMAKE_POSITION_INDEPENDENT_CODE': get_option('b_staticpic'),
			'CMAKE_INTERPROCEDURAL_OPTIMIZATION': get_option('b_lto'),
			'CMAKE_POLICY_DEFAULT_CMP0069': 'NEW',
			'MSVC_RUNTIME_LIBRARY': 'MultiThreaded',
			'BUILD_SHARED_LIBS': false,
			'BUILD_CURL_EXE': false,
			'CURL_LTO': get_option('b_lto'),
			'HTTP_ONLY': true,
			'CURL_DISABLE_COOKIES': true,
			'CURL_DISABLE_VERBOSE_STRINGS': true,
			'ENABLE_UNIX_SOCKETS': false,
			'OPENSSL_USE_STATIC_LIBS': true,
			'OPENSSL_MSVC_STATIC_RT': true
		})
		if host_machine.system() == 'windows'
			libcurl_options.add_cmake_defines({
				'CURL_STATIC_CRT': true,
				'CURL_USE_SCHANNEL': true,
				'CURL_TARGET_WINDOWS_VERSION': '0x0A00' # Win 10
			})
			extra_deps += compiler.find_library('crypt32')
			extra_deps += compiler.find_library('ws2_32')
		else
			# Required by static OpenSSL
			extra_deps += compiler.find_library('dl', required: false)
		endif
		libcurl_options.set_install(false)
		libcurl_options.append_compile_args('c', '-DOPENSSL_NO_SSL3_METHOD', '-DCURL_STATICLIB')
		libcurl_dep = cmake.subproject('curl', options: libcurl_options).dependency('libcurl')
	else
		libcurl_dep = cmake.subproject('curl').dependency('libcurl')
	endif
endif

extra_args = []

if host_machine.system() == 'windows'
	default_library = get_option('default_library')
	if default_library == 'both'
		error('default_library=both is not supported on Windows')
	elif default_library == 'shared'
		extra_args += '-DDDNS_SHARED_LIB'
	endif
endif

# Put kwargs unsupported by muon in a dictionary so that they get used only
# when building with meson
muon_unsupported_kwargs = {}
if not get_option('muon')
	muon_unsupported_kwargs = {
		'override_options': 'cpp_eh=none'
	}
endif

libcloudflare_ddns = library(
	'cloudflare-ddns',
	'lib'/'cloudflare-ddns.cpp',
	cpp_args: extra_args,
	dependencies: [
		libcurl_dep,
		extra_deps
	],
	extra_files: 'include'/'ddns'/'cloudflare-ddns.h',
	gnu_symbol_visibility: 'hidden',
	include_directories: 'include',
	install: true,
	version: meson.project_version(),
	kwargs: muon_unsupported_kwargs
)

cloudflare_ddns_dep = declare_dependency(
	compile_args: extra_args,
	include_directories: 'include',
	link_with: libcloudflare_ddns,
	version: meson.project_version()
)

install_subdir(
	'include'/'ddns',
	install_dir: get_option('includedir')
)

if get_option('executable')
	subdir('exe')
endif

if get_option('tests')
	subdir('tests')
endif

import('pkgconfig').generate(
	libcloudflare_ddns,
	description: 'Simple utility to dynamically change a DNS record using Cloudflare',
	extra_cflags: extra_args,
	url: 'https://github.com/Tachi107/cloudflare-ddns'
)
