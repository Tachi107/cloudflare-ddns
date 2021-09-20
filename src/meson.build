# SPDX-FileCopyrightText: 2021 Andrea Pappacoda
#
# SPDX-License-Identifier: AGPL-3.0-or-later

configure_file(
	input: 'config_path.hpp.in',
	output: 'config_path.hpp',
	configuration: {'sysconfdir': get_option('prefix')/get_option('sysconfdir')}
)
