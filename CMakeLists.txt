cmake_minimum_required (VERSION 2.9)
project (Tacticus CXX C)

set(TARGET_NAME "Tacticus")

if (MSVC_VERSION GREATER_EQUAL "1900")
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
    if (_cpp_latest_flag_supported)
        add_compile_options("/std:c++latest")
    endif()
endif()

add_compile_options("/WX")
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")


set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
include(InstallRequiredSystemLibraries)

set (VERSIONING_PREFIX "TACTICUS")
INCLUDE(cmake/versioning.cmake)

set(SFML_STATIC_LIBRARIES TRUE)
find_package(SFML 2 COMPONENTS system window graphics audio REQUIRED)
include("cmake/EmbedFileFTO.cmake")

include_directories(${SFML_INCLUDE_DIR})
include_directories(${CEREAL_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_BINARY_DIR})

#embed_file("${PROJECT_SOURCE_DIR}/assets/Paul_10x10.png")
embed_file("${PROJECT_SOURCE_DIR}/assets/cp437_12x12.png")

SET(PROJECT_HEADERS
    src/common.hpp
    src/console.hpp
    src/map.hpp
    src/pcg_extras.hpp
    src/pcg_random.hpp
    src/pcg_uint128.hpp
    src/version.hpp
    src/miniz.h
    src/worker.hpp
	src/action.hpp
	src/knowledge.hpp
	src/e_furniture.hpp
	src/gui.hpp
    src/room.hpp
	)

SET(PROJECT_SOURCES
    src/common.cpp
    src/console.cpp
    src/map.cpp
    src/miniz.c
    src/worker.cpp
    src/gui.cpp
    src/room.cpp
	src/e_furniture.cpp
	)
add_executable(${TARGET_NAME} src/main.cpp ${PROJECT_HEADERS} ${PROJECT_SOURCES} ${EMBEDDED_HDRS} ${EMBEDDED_LIBS})

target_link_libraries(${TARGET_NAME} ${SFML_LIBRARIES} ${SFML_DEPENDENCIES} )


#add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets ${PROJECT_BINARY_DIR}/assets
#)

install(TARGETS ${TARGET_NAME} DESTINATION .)
install(DIRECTORY docs DESTINATION .)