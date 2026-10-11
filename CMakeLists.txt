cmake_minimum_required(VERSION 3.21)

# On Windows, use vcpkg (manifest in vcpkg.json) for CommonLibF4's dependencies.
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
	set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")
endif()
project(WitcherCamera VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Match CommonLibF4's runtime so the core library links into the plugin.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# ---- Engine-free core (builds anywhere) -----------------------------------
add_library(witcher_core STATIC
	core/src/camera_rig.cpp
	core/src/camera_system.cpp
	core/src/directional.cpp
	core/src/lock_on.cpp
	core/src/settings.cpp
	core/src/targeting.cpp
)
target_include_directories(witcher_core PUBLIC core/include)
if(MSVC)
	target_compile_options(witcher_core PRIVATE /W4 /permissive-)
else()
	target_compile_options(witcher_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- Tests -----------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
	add_executable(witcher_tests tests/tests.cpp)
	target_link_libraries(witcher_tests PRIVATE witcher_core)
	target_compile_definitions(witcher_tests PRIVATE WITCHER_CONFIG_INI="${CMAKE_CURRENT_SOURCE_DIR}/config/WitcherCamera.ini")
	add_test(NAME witcher_tests COMMAND witcher_tests)
endif()

# ---- F4SE plugin (Windows, Fallout 4 1.10.163 / F4SE 0.6.23) -------------
# Needs the original (old-gen) CommonLibF4, which exports the
# F4SEPlugin_Query/F4SEPlugin_Load entry points F4SE 0.6.x requires.
# See README.md for setup.
option(WITCHER_BUILD_PLUGIN "Build the F4SE plugin DLL" ${WIN32})
set(COMMONLIBF4_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibF4" CACHE PATH "CommonLibF4 checkout")

if(WITCHER_BUILD_PLUGIN)
	if(NOT TARGET CommonLibF4)
		add_subdirectory("${COMMONLIBF4_DIR}/CommonLibF4" CommonLibF4 EXCLUDE_FROM_ALL)
	endif()
	find_package(fmt REQUIRED CONFIG)
	find_package(spdlog REQUIRED CONFIG)

	add_library(WitcherCamera SHARED
		plugin/src/main.cpp
		plugin/src/Engine.cpp
		plugin/src/Input.cpp
		plugin/src/Movement.cpp
		plugin/src/Plugin.cpp
	)
	target_include_directories(WitcherCamera PRIVATE plugin/src)
	target_link_libraries(WitcherCamera PRIVATE witcher_core CommonLibF4::CommonLibF4 fmt::fmt spdlog::spdlog)
	target_precompile_headers(WitcherCamera PRIVATE plugin/src/PCH.h)
	target_compile_definitions(WitcherCamera PRIVATE
		_UNICODE
		PLUGIN_NAME="WitcherCamera"
		PLUGIN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		PLUGIN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
		PLUGIN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
	)

	if(DEFINED ENV{Fallout4Path})
		add_custom_command(TARGET WitcherCamera POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:WitcherCamera> "$ENV{Fallout4Path}/Data/F4SE/Plugins/"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/config/WitcherCamera.ini" "$ENV{Fallout4Path}/Data/F4SE/Plugins/"
		)
	endif()
endif()
