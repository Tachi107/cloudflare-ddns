project(
	'cloudflare-ddns',
	'cpp',
	default_options: [
		'cpp_std=c++17',
		'buildtype=release',
		'b_ndebug=if-release',
		'b_lto=true'
	],
	version: '0.4.0',
	license: 'AGPL-3.0-or-later',
	meson_version: '>=0.49.0'
)

compiler = meson.get_compiler('cpp')

if compiler.get_id().contains('clang')
	add_project_arguments(['-fuse-ld=lld', '-Wno-unused-command-line-argument'], language: 'cpp')
	add_project_link_arguments('-fuse-ld=lld', language: 'cpp')
endif

cmake = import('cmake')

libcurl_dep = dependency('libcurl', required: false)
if not libcurl_dep.found()
	if meson.version().version_compare('>=0.55.0')
		libcurl_options = cmake.subproject_options()
		libcurl_options.add_cmake_defines(
			{'CMAKE_BUILD_TYPE': get_option('buildtype')},
			{'MSVC_RUNTIME_LIBRARY': 'MultiThreaded'},
			{'BUILD_SHARED_LIBS': false},
			{'CURL_STATIC_CRT': true},
			{'CURL_TARGET_WINDOWS_VERSION': '0x0A00'}, # Win 10
			{'BUILD_CURL_EXE': false},
			{'CURL_LTO': true},
			{'HTTP_ONLY': true},
			{'CURL_DISABLE_COOKIES': true},
			{'CURL_DISABLE_VERBOSE_STRINGS': true},
			{'ENABLE_UNIX_SOCKETS': false}
		)
		if host_machine.system() == 'windows'
			libcurl_options.add_cmake_defines(
				{'CMAKE_USE_SCHANNEL': true}
			)
		endif
		if compiler.get_argument_syntax() == 'gcc'
			libcurl_options.append_compile_args('c', '-DOPENSSL_NO_SSL3_METHOD -DCURL_STATICLIB')
		elif compiler.get_argument_syntax() == 'msvc'
			libcurl_options.append_compile_args('c', '/DOPENSSL_NO_SSL3_METHOD /CURL_STATICLIB')
		endif
		libcurl_dep = cmake.subproject('curl', options: libcurl_options).dependency('libcurl')
	else
		libcurl_dep = cmake.subproject('curl').dependency('libcurl')
	endif
endif

simdjson_dep = dependency('simdjson', required: false)
if not simdjson_dep.found()
	if meson.version().version_compare('>=0.55.0')
		simdjson_options = cmake.subproject_options()
		simdjson_options.add_cmake_defines(
			{'CMAKE_BUILD_TYPE': get_option('buildtype')},
			{'MSVC_RUNTIME_LIBRARY': 'MultiThreaded'},
			{'BUILD_SHARED_LIBS': false},
			{'SIMDJSON_JUST_LIBRARY': true},
			{'SIMDJSON_BUILD_STATIC': true},
			{'SIMDJSON_WINDOWS_DLL': false},
			{'SIMDJSON_USING_WINDOWS_DYNAMIC_LIBRARY': false}
		)
		if compiler.get_argument_syntax() == 'msvc'
			simdjson_options.append_compile_args('c', '/MT')
			simdjson_options.append_compile_args('cpp', '/MT')
		endif
		simdjson_dep = cmake.subproject('simdjson', options: simdjson_options).dependency('simdjson')
	else
		simdjson_dep = cmake.subproject('simdjson').dependency('simdjson')
	endif
endif

yaml_cpp_dep = dependency('yaml-cpp', required: false)
if not yaml_cpp_dep.found()
	if meson.version().version_compare('>=0.55.0')
		yaml_cpp_options = cmake.subproject_options()
		yaml_cpp_options.add_cmake_defines(
			{'CMAKE_BUILD_TYPE': get_option('buildtype')},
			{'MSVC_RUNTIME_LIBRARY': 'MultiThreaded'},
			{'BUILD_SHARED_LIBS': false},
			{'YAML_MSVC_SHARED_RT': false},
			{'YAML_CPP_BUILD_TOOLS': false},
			{'YAML_CPP_BUILD_TESTS': false},
			{'YAML_CPP_INSTALL': false}
		)
		yaml_cpp_dep = cmake.subproject('yaml-cpp', options: yaml_cpp_options).dependency('yaml-cpp')
	else
		simdjson_dep = cmake.subproject('yaml-cpp').dependency('yaml-cpp')
	endif
endif

executable(
	'cloudflare-ddns',
	sources: 'cloudflare-ddns/main.cpp',
	dependencies: [
		libcurl_dep,
		simdjson_dep,
		dependency('threads'),
		yaml_cpp_dep,
		compiler.find_library('ws2_32', required: false)
	],
	install: true
)

if host_machine.system() == 'linux' or host_machine.system() == 'openbsd'
	install_data('config.yaml', install_dir: '/etc/cloudflare-ddns')
elif host_machine.system() == 'freebsd'
	install_data('config.yaml', install_dir: '/usr/local/etc/cloudflare-ddns')
else
	install_data('config.yaml')
endif
