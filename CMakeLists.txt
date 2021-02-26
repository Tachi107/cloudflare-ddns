cmake_minimum_required(VERSION 3.15)

project(cloudflare-ddns)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options(-fuse-ld=lld -Wno-unused-command-line-argument)
	add_link_options(-fuse-ld=lld)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_result OUTPUT ipo_output)
if(ipo_result)
	cmake_policy(SET CMP0069 NEW)
	set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

include(FetchContent)

FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/whoshuu/cpr.git GIT_TAG 1.5.2)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(BUILD_CPR_TESTS OFF CACHE BOOL "")
add_compile_definitions(OPENSSL_NO_SSL3_METHOD)
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(json GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent.git GIT_TAG v3.9.1)
FetchContent_GetProperties(json)
if(NOT json_POPULATED)
	FetchContent_Populate(json)
	add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(simdjson GIT_REPOSITORY https://github.com/simdjson/simdjson.git GIT_TAG v0.8.2 GIT_SHALLOW TRUE)
set(SIMDJSON_JUST_LIBRARY ON CACHE INTERNAL "")
set(SIMDJSON_BUILD_STATIC ON CACHE INTERNAL "")
FetchContent_MakeAvailable(simdjson)

add_executable(${PROJECT_NAME} cloudflare-ddns/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr nlohmann_json::nlohmann_json simdjson)
