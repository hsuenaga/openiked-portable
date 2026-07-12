# Copyright (c) 2020-2022 Tobias Heider <tobhe@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

cmake_minimum_required(VERSION 3.12)

if (APPLE)
project(openiked LANGUAGES C Swift)
else()
project(openiked LANGUAGES C)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckIncludeFiles)
include(CheckLinkerFlag)

option(BUILD_LIBRARY "create library instaed of excutable." OFF)
option(THREAD "use thread instead of process(macOS)." OFF)
option(HOMEBREW "use libraries installed by Homebrew(macOS)." OFF)
option(OSSL_LEGACY "use OpenSSL legacy provider." OFF)
option(ASAN "Enable Address SANitizer." OFF)
option(UBSAN "Enable Undefined SANitizer." OFF)
option(MSAN "Enable Memory SANitizer." OFF)
option(CLUSTERFUZZ "Enable libfuzzer." OFF)

if(CMAKE_BUILD_TYPE STREQUAL DEBUG)
	message("Debug build.")
elseif(CMAKE_BUILD_TYPE STREQUAL RELEASE)
	message("Release build.")
endif()

include(Configure.cmake)

add_subdirectory(compat)
add_subdirectory(iked)
if (APPLE AND BUILD_LIBRARY)
add_subdirectory(swift)
endif()
if (NOT CMAKE_SYSTEM_NAME MATCHES "iOS")
add_subdirectory(ikectl)
endif()
if (NOT BUILD_LIBRARY)
# Not supported by library-based openiked.
add_subdirectory(regress/dh)
add_subdirectory(regress/parser)
add_subdirectory(regress/test_helper)
endif()
