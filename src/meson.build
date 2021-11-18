# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
#
# SPDX-License-Identifier: AGPL-3.0-or-later

# See https://mesonbuild.com/Builtin-options.html#universal-options
sysconfdir = get_option('prefix')/get_option('sysconfdir')
if get_option('prefix') == '/usr'
	sysconfdir = get_option('sysconfdir')
endif

tomlplusplus_dep = dependency(
	'tomlplusplus',
	fallback: ['tomlplusplus', 'tomlplusplus_dep'],
	default_options: ['default_library=static']
)

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
	install: true,
	sources: configure_file(
		input: 'config_path.hpp.in',
		output: 'config_path.hpp',
		configuration: {'sysconfdir': sysconfdir}
	)
)

install_data('..'/'config.toml', install_dir: get_option('sysconfdir')/'cloudflare-ddns')
