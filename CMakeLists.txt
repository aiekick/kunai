cmake_minimum_required(VERSION 3.20)

set(PROJECT kunai)

project(${PROJECT} CXX)
enable_language(C CXX)

## for group smake targets in the dir CmakeTargets
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CmakeTargets")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## some defines for debug mode (before 3rdparty.cmake)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

set(FINAL_BIN_DIR ${CMAKE_SOURCE_DIR}/bin)
if(MSVC)
	set(CMAKE_PDB_OUTPUT_DIRECTORY_DEBUG "${FINAL_BIN_DIR}/debug")
	set(CMAKE_PDB_OUTPUT_DIRECTORY_RELEASE "${FINAL_BIN_DIR}/release")
	set(CMAKE_PDB_OUTPUT_DIRECTORY_MINSIZEREL "${FINAL_BIN_DIR}/minsizerel")
	set(CMAKE_PDB_OUTPUT_DIRECTORY_RELWITHDEBINFO "${FINAL_BIN_DIR}/reldeb")
endif()

set(CMAKE_INSTALL_PREFIX .)
set(CMAKE_INSTALL_BINDIR .)

set(CMAKE_DEBUG_POSTFIX _debug)
set(CMAKE_RELEASE_POSTFIX)
set(CMAKE_MINSIZEREL_POSTFIX _minsizerel)
set(CMAKE_RELWITHDEBINFO_POSTFIX _reldeb)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zo /std:c++17")
    add_definitions(-D_USE_MATH_DEFINES -DMSVC -DNOMINMAX)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -Wall -std=c++17 -Wextra -Wpedantic -Wunused-parameter -Wno-unknown-pragmas")
    if((CMAKE_BUILD_TYPE STREQUAL "Debug") OR ($<CONFIG> STREQUAL "Debug"))
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    endif()
endif()

include(cmake/sqlite3.cmake)

include_directories(3rdparty)

file(GLOB_RECURSE SRC_SOURCES ${CMAKE_SOURCE_DIR}/src/*.*)
source_group(TREE ${CMAKE_SOURCE_DIR}/src PREFIX src FILES ${SRC_SOURCES})

if(MSVC)
    file(GLOB_RECURSE EZ_LIBS_SOURCES ${CMAKE_SOURCE_DIR}/3rdparty/ezlibs/*.hpp)
    source_group(TREE ${CMAKE_SOURCE_DIR}/3rdparty/ezlibs PREFIX ezlibs FILES ${EZ_LIBS_SOURCES})
endif()

add_executable(${PROJECT} 
    ${EZ_LIBS_SOURCES}
    ${SRC_SOURCES}
)

target_link_libraries(${PROJECT} PRIVATE
    ${SQLITE3_LIBRARIES}
)

target_include_directories(${PROJECT} PRIVATE
    ${SQLITE3_INCLUDE_DIR}
    ${EZLIBS_INCLUDE_DIR}
    src
)

set_target_properties(${PROJECT} PROPERTIES
	OUTPUT_NAME "${PROJECT}"
)

if(MSVC)
    set_property(TARGET ${PROJECT} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_target_properties(${PROJECT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${FINAL_BIN_DIR})
set_target_properties(${PROJECT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${FINAL_BIN_DIR})
set_target_properties(${PROJECT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${FINAL_BIN_DIR})
set_target_properties(${PROJECT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${FINAL_BIN_DIR})
set_target_properties(${PROJECT} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${FINAL_BIN_DIR})

########################################################
################# BUILD INCREMENT TOOL #################
########################################################

add_subdirectory(3rdparty/ezBuildInc)
set_target_properties(ezBuildInc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_custom_command(
	TARGET ${PROJECT} PRE_BUILD
	COMMAND ezBuildInc ${PROJECT} ${CMAKE_SOURCE_DIR}/src/app/headers/${PROJECT}Build.h
)
