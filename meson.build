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

libcurl_dep = dependency('libcurl', default_options: [
	'default_library=static',

	'tool=disabled',
	'tests=disabled',
	'unittests=disabled',
])


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
	dependencies: [libcurl_dep],
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
