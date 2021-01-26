project (ealayer3)
cmake_minimum_required (VERSION 2.8)
enable_testing ()

# Set our version number
set (ealayer3_VERSION_MAJOR 0)
set (ealayer3_VERSION_MINOR 7)
set (ealayer3_VERSION_PATCH 1)

# Find boost and include it
find_package (Boost 1.36.0 REQUIRED)
include_directories (${Boost_INCLUDE_DIRS})

# Find mpg123 and include it
find_library (MPG123_LIBRARY
              NAMES mpg123
              PATHS /usr/local/lib /usr/lib
              )
find_path (MPG123_INCLUDES mpg123.h
           /usr/local/include /usr/include
           )
include_directories (${MPG123_INCLUDES})

# Configure our version file
configure_file ("${PROJECT_SOURCE_DIR}/src/Version.h.in"
                "${PROJECT_BINARY_DIR}/Version.h"
                )
                
# Make sure that we can include it
include_directories("${PROJECT_BINARY_DIR}")

# The list of source files
set (SOURCE_FILES
    src/Main.cpp
    src/FileDecoder.cpp
    
    src/BlockLoader.cpp
    src/Parser.cpp
    src/MpegGenerator.cpp
    src/OutputStream.cpp
    src/MpegOutputStream.cpp
    src/PcmOutputStream.cpp
    src/WaveWriter.cpp
    src/AllFormats.cpp
    src/MpegParser.cpp
    src/Generator.cpp
    src/BlockWriter.cpp

    src/Loaders/HeaderlessLoader.cpp
    src/Loaders/SingleBlockLoader.cpp
    src/Loaders/SCxLoader.cpp
    src/Loaders/AsfGstrLoader.cpp
    src/Loaders/AsfPtLoader.cpp
    src/Loaders/HeaderBLoader.cpp

    src/Parsers/ParserForSCx.cpp
    src/Parsers/ParserVersion6.cpp

    src/Writers/HeaderlessWriter.cpp
    src/Writers/SingleBlockWriter.cpp
    src/Writers/HeaderBWriter.cpp
    )

add_executable (ealayer3 ${SOURCE_FILES})
target_link_libraries (ealayer3 ${MPG123_LIBRARY})

# Add support for tests
file (GLOB FILES_TO_TEST files/*)
set (TEST_SOURCE_FILES ${SOURCE_FILES} src/TestDriver.cpp)
list (REMOVE_ITEM TEST_SOURCE_FILES src/Main.cpp)
add_executable (ealayer3testdriver ${TEST_SOURCE_FILES})
target_link_libraries (ealayer3testdriver ${MPG123_LIBRARY})

foreach (TEST_FILE ${FILES_TO_TEST})
    get_filename_component (TEST_NAME ${TEST_FILE} NAME)
    add_test (${TEST_NAME} ealayer3testdriver ${TEST_FILE})
endforeach (TEST_FILE)

# Install targets
if (WIN32)
    install (TARGETS ealayer3 DESTINATION .)
    install (FILES "${PROJECT_SOURCE_DIR}/mpg123license.txt" DESTINATION .)
    install (FILES "${PROJECT_SOURCE_DIR}/license.txt" DESTINATION .)
    install (FILES "${PROJECT_SOURCE_DIR}/win32/libmpg123.dll" DESTINATION .)
else (WIN32)
	include (InstallRequiredSystemLibraries)
    install (TARGETS ealayer3 DESTINATION bin)
endif (WIN32)

# CPack
set (CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/license.txt")
set (CPACK_PACKAGE_VERSION_MAJOR "${ealayer3_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${ealayer3_VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${ealayer3_VERSION_PATCH}")
set (CPACK_GENERATOR "ZIP")
include (CPack)
