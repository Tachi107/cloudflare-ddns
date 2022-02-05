# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
#
# SPDX-License-Identifier: AGPL-3.0-or-later

# See https://mesonbuild.com/Builtin-options.html#universal-options
sysconfdir = get_option('prefix')/get_option('sysconfdir')
if get_option('prefix') == '/usr'
	sysconfdir = get_option('sysconfdir')
endif

INIReader_dep = dependency(
	'INIReader',
	fallback: ['inih', 'INIReader_dep'],
	default_options: ['default_library=static', 'distro_install=false']
)

executable(
	'cloudflare-ddns',
	'main.cpp',
	dependencies: [
		cloudflare_ddns_dep,
		libcurl_dep,
		simdjson_dep,
		INIReader_dep,
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

install_data(
	'config.ini',
	install_dir: get_option('sysconfdir')/'cloudflare-ddns',
	install_mode: 'rw-------'
)

systemddir = 'lib'/'systemd'/'system'

configure_file(
	input: 'cloudflare-ddns.service.in',
	output: 'cloudflare-ddns.service',
	configuration: {'bindir': get_option('prefix')/get_option('bindir')},
	install: true,
	install_dir: systemddir
)

install_data(
	'cloudflare-ddns.timer',
	install_dir: systemddir
)
