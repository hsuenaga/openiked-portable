set(SRCS)
set(LIBS)
set(CFLAGS)
set(INC_DIRS)
set(LIB_DIRS)

add_library(swiftike STATIC
	proc.c
	ipsec.c
	pfkey.c
)
set_target_properties(swiftike PROPERTIES
	SWIFT_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/bridge.h"
)
target_include_directories(swiftike PRIVATE
	../iked
	../compat
	$ENV{DESTDIR}/usr/include
)
target_link_directories(swiftike PRIVATE
	../iked
	../compat
	$ENV{DESTDIR}/usr/lib
)
target_link_libraries(swiftike PRIVATE
	iked
)
target_compile_options(swiftike PRIVATE 
	-fblocks
)

set(BRIDGING_HEADER_PATH
	 "${CMAKE_CURRENT_SOURCE_DIR}/bridge.h"
)
# https://cmake.org/cmake/help/latest/prop_sf/MACOSX_PACKAGE_LOCATION.html
set(OPENIKED_RESOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/etc
)
set_source_files_properties(${OPENIKED_RESOURCES} PROPERTIES
	MACOSX_PACKAGE_LOCATION "Resources"
)
add_executable(openiked
	main.swift
	bridge.h
	swift_bridge.c
	swift_bridge.h
	${OPENIKED_RESOURCES}
)
target_link_directories(openiked PRIVATE
	../iked
	../compat
	$ENV{DESTDIR}/usr/lib
)
target_include_directories(openiked PRIVATE
	../iked
	../compat
	$ENV{DESTDIR}/usr/include
)
target_link_libraries(openiked PRIVATE
	iked
	iked-shared
	compat-static
	swiftike
	event_pthreads
)
set_target_properties(openiked PROPERTIES
        XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${BRIDGING_HEADER_PATH}"
	MACOSX_BUNDLE TRUE
	MACOSX_BUNDLE_GUI_IDENTIFIER "jp.ad.iij.iedgesase"
	MACOSX_BUNDLE_BUNDLE_NAME "openiked"
	MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
	MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
)
if(NOT XCODE)
#https://github.com/Kitware/CMake/blob/master/Tests/SwiftMix/CMakeLists.txt
target_compile_options(openiked PRIVATE
        $<$<COMPILE_LANGUAGE:Swift>:-import-objc-header;${BRIDGING_HEADER_PATH}>
)
endif()
