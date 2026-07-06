#set (INC_DIRS
#	${CMAKE_CURRENT_SOURCE_DIR}
#	${CMAKE_CURRENT_SOURCE_DIR}/../iked
#)
#if (DEFINED ENV{DESTDIR})
#	list(APPEND INC_DIRS
#		$ENV{DESTDIR}/usr/include
#	)
#endif()

#set (LIB_DIRS
#	${CMAKE_CURRENT_SOURCE_DIR}
#	${CMAKE_CURRENT_SOURCE_DIR}/../iked
#)
#if (DEFINED ENV{DESTDIR})
#	list(APPEND INC_DIRS
#		$ENV{DESTDIR}/usr/lib
#	)
#endif()

set(SRCS)
set(LIBS)
set(CFLAGS)
set(INC_DIRS)
set(LIB_DIRS)

add_library(swiftike STATIC swift_bridge.c)

target_include_directories(swiftike PUBLIC
	../iked
)

target_link_libraries(swiftike PUBLIC
	iked
)

add_executable(openiked
	main.swift
)

target_link_libraries(openiked PUBLIC
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
