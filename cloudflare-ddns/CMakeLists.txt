cmake_minimum_required(VERSION 3.14)

project(cloudflare-ddns)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_result OUTPUT ipo_output)
if(ipo_result)
	cmake_policy(SET CMP0069 NEW)
	set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

add_compile_definitions(OPENSSL_NO_SSL3_METHOD)

find_package(Threads)

include(FetchContent)

FetchContent_Declare(curl GIT_REPOSITORY https://github.com/curl/curl.git GIT_TAG curl-7_75_0)
set(BUILD_CURL_EXE OFF CACHE BOOL "")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(CURL_LTO ON CACHE BOOL "")
set(HTTP_ONLY ON CACHE BOOL "")
set(CURL_DISABLE_COOKIES ON CACHE BOOL "")
set(CURL_DISABLE_VERBOSE_STRINGS ON CACHE BOOL "")
set(ENABLE_UNIX_SOCKETS OFF CACHE BOOL "")
FetchContent_MakeAvailable(curl)

FetchContent_Declare(simdjson GIT_REPOSITORY https://github.com/simdjson/simdjson.git GIT_TAG v0.8.2 GIT_SHALLOW TRUE)
set(SIMDJSON_JUST_LIBRARY ON CACHE INTERNAL "")
set(SIMDJSON_BUILD_STATIC ON CACHE INTERNAL "")
FetchContent_MakeAvailable(simdjson)

add_executable(${PROJECT_NAME} cloudflare-ddns/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads simdjson libcurl)
