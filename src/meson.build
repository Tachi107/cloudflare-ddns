# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
#
# SPDX-License-Identifier: AGPL-3.0-or-later

# See https://mesonbuild.com/Builtin-options.html#universal-options
sysconfdir = get_option('prefix')/get_option('sysconfdir')
if get_option('prefix') == '/usr'
	sysconfdir = get_option('sysconfdir')
endif

configure_file(
	input: 'config_path.hpp.in',
	output: 'config_path.hpp',
	configuration: {'sysconfdir': sysconfdir}
)
