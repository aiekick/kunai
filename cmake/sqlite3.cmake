set(SQLITE3_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/sqlite3)

if(MSVC)
	set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE)
endif()

add_library(sqlite3 STATIC
	${SQLITE3_INCLUDE_DIR}/sqlite3.c
	${SQLITE3_INCLUDE_DIR}/sqlite3.h
	${SQLITE3_INCLUDE_DIR}/sqlite3ext.h
)

set_target_properties(sqlite3 PROPERTIES FOLDER 3rdparty/Static)

set_target_properties(sqlite3 PROPERTIES LINKER_LANGUAGE C)

set_target_properties(sqlite3 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FINAL_BIN_DIR}")
set_target_properties(sqlite3 PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${FINAL_BIN_DIR}")
set_target_properties(sqlite3 PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${FINAL_BIN_DIR}")
set_target_properties(sqlite3 PROPERTIES RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${FINAL_BIN_DIR}")
set_target_properties(sqlite3 PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${FINAL_BIN_DIR}")

if(MSVC)
	set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE)
	set_property(TARGET sqlite3 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (LINUX)
	target_link_libraries(sqlite3 PRIVATE pthread dl)
endif()

set(SQLITE3_LIBRARIES sqlite3)
