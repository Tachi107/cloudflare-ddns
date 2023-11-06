# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
#
# SPDX-License-Identifier: AGPL-3.0-or-later

# If get_option('sysconfdir') returns an absolute path,
# get_option('prefix')/get_option('sysconfdir') simply returns the
# correct absolute sysconfdir, stripping away the prefix.
# This means that concatenating the prefix with the sysconfdir always
# yelds an absolute path that is correct, regardless of the fact that
# sysconfdir is absolute or not (e.g. /etc vs /usr/local/etc).
# Source: https://github.com/mesonbuild/meson/pull/9903#issuecomment-1071118017
# and: https://mesonbuild.com/Reference-manual_functions.html#join_paths

sysconfdir = get_option('prefix')/get_option('sysconfdir')

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
		INIReader_dep
	],
	gnu_symbol_visibility: 'hidden',
	install: true,
	sources: configure_file(
		input: 'paths.hpp.in',
		output: 'paths.hpp',
		configuration: {
			'cache_dir': get_option('prefix')/get_option('localstatedir')/'cache',
			'sysconfdir': sysconfdir
		}
	)
)

install_data(
	'config.ini',
	install_dir: get_option('sysconfdir')/'cloudflare-ddns',
	install_mode: ['rw-------', 'cloudflare-ddns', 'cloudflare-ddns']
)

# https://github.com/mesonbuild/meson/issues/1550
# https://github.com/mesonbuild/meson/pull/9342
install_man(
	configure_file(
		input: 'cloudflare-ddns.1.in',
		output: 'cloudflare-ddns.1',
		configuration: {
			'sysconfdir': sysconfdir
		}
	)
)

systemd = dependency('systemd', required: false)
if systemd.found()
	systemd_system_unit_dir = systemd.get_variable(pkgconfig: 'systemdsystemunitdir')
	sysusers_dir = systemd.get_variable(pkgconfig: 'sysusersdir')

	configure_file(
		input: 'systemd'/'cloudflare-ddns.service.in',
		output: 'cloudflare-ddns.service',
		configuration: {
			'bindir': get_option('prefix')/get_option('bindir'),
			'libdir': get_option('prefix')/get_option('libdir')
		},
		install: true,
		install_dir: systemd_system_unit_dir
	)

	install_data(
		'systemd'/'cloudflare-ddns.timer',
		install_dir: systemd_system_unit_dir
	)

	install_data(
		'sysusers.d'/'cloudflare-ddns.conf',
		install_dir: sysusers_dir
	)
endif
