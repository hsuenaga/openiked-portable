set(SRCS)
set(LIBS)
set(CFLAGS)
set(INC_DIRS)
set(LIB_DIRS)

add_library(swiftike STATIC
	proc.c
	swift_bridge.c
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

target_compile_options(swiftike PRIVATE -fblocks)

add_executable(openiked
	main.swift
	bridge.h
)

target_link_directories(openiked PRIVATE
	../iked
	../compat
	$ENV{DESTDIR}/usr/lib
)

target_link_libraries(openiked PRIVATE
	iked
	iked-shared
	compat
	swiftike
)

#target_include_directories(openiked PRIVATE ${INC_DIRS})
#target_link_directories(openiked PRIVATE ${LIB_DIRS})
#target_link_libraries(openiked PRIVATE ${LIBS})
#target_compile_options(openiked PRIVATE ${CFLAGS})
#set_target_properties(iked PROPERTIES PUBLIC_HEADER "swift_bridge.h")
#
if(XCODE)
set_target_properties(openiked PROPERTIES
        XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/bridge.h"
)
else()
target_compile_options(openiked PRIVATE
        $<$<COMPILE_LANGUAGE:Swift>:-import-objc-header;${CMAKE_CURRENT_SOURCE_DIR}/bridge.h>
)
endif()
