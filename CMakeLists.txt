cmake_minimum_required(VERSION 3.10...3.32)
project(c4)
option(SHAREDLIB "if true the shared library will be build" OFF)
option(TEST "if true the test will be build" OFF)
option(COVERAGE "if true the coverage will be build" OFF)
option(WASM "if true the wasm will be build" OFF)
option(KOTLIN "if true the kotlin bindings will be build" OFF)
option(SWIFT "if true the swift bindings will be build" OFF)
option(BLS_DESERIALIZE "if true bls keys will be stored deserialized, which increases the state per sync committee period from 25kB to 50kB, but make verification 5x faster" ON)

if(KOTLIN)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CURL OFF)
endif()

if(SWIFT)
    # Set SDK paths based on target architecture
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        execute_process(
            COMMAND xcrun --sdk iphonesimulator --show-sdk-path
            OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(IOS_PLATFORM "SIMULATOR64")
        set(TARGET_TRIPLE "x86_64-apple-ios-simulator")
    elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        execute_process(
            COMMAND xcrun --sdk iphoneos --show-sdk-path
            OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(IOS_PLATFORM "OS64")
        set(TARGET_TRIPLE "arm64-apple-ios")
    endif()

    # Set common iOS build settings
    set(CMAKE_SYSTEM_NAME "iOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum iOS deployment version" FORCE)
    set(CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH NO)
    set(CMAKE_IOS_INSTALL_COMBINED NO)
    
    # Set iOS-specific compiler and linker flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -target ${TARGET_TRIPLE} -isysroot ${CMAKE_OSX_SYSROOT}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -target ${TARGET_TRIPLE} -isysroot ${CMAKE_OSX_SYSROOT}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -target ${TARGET_TRIPLE}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -target ${TARGET_TRIPLE}")
    
    # Disable CURL for iOS builds
    set(CURL OFF)
endif()

if(CURL)
    add_definitions(-DUSE_CURL)
endif()

if(BLS_DESERIALIZE)
    add_definitions(-DBLS_DESERIALIZE)
endif()

if(TEST)
    if(NOT DEFINED TESTDATA_DIR)
        set(TESTDATA_DIR "${CMAKE_SOURCE_DIR}/test/data")
    endif()
    add_definitions(-DTEST -DTESTDATA_DIR="${TESTDATA_DIR}")
endif()

if(COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

set(OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)

if(CMAKE_BUILD_TYPE_UPPER STREQUAL "MINSIZEREL")
    if(POLICY CMP0069)
        cmake_policy(SET CMP0069 NEW)
    endif()
    # Activate Link-Time Optimization
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    # Compiler flags for size optimization
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        if(APPLE)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -dead_strip")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -dead_strip")
        else()
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --gc-sections")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --gc-sections")
        endif()
    endif()
endif()

add_subdirectory(src)
add_subdirectory(libs)

if(TEST)
    enable_testing()
    add_subdirectory(test)
    set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
endif()

add_custom_target(doc 
    COMMAND ${CMAKE_COMMAND} -E echo "Generating documentation..."
    COMMAND ${CMAKE_COMMAND} -E env node ${CMAKE_SOURCE_DIR}/scripts/update_doc.js
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/scripts
    COMMENT "Running update_doc.js to generate documentation"
)

add_custom_target(valgrind
    COMMAND ${CMAKE_COMMAND} -E echo "Generating valgrind image..."
    COMMAND ${CMAKE_COMMAND} -E env bash -c "docker build -f test/valgrind/Dockerfile --platform=linux/amd64 -t c4_valgrind ."
    COMMAND ${CMAKE_COMMAND} -E env bash -c "docker run --rm --platform=linux/amd64 c4_valgrind"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating the valgrind image"
)

if(SHAREDLIB)
    add_library(c4Lib SHARED src/util/version.c)
    target_link_libraries(c4Lib PRIVATE crypto verifier util blst)
    set_target_properties(c4Lib PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY}
        VERSION 1.0
        SOVERSION 1
    )
endif()

if(WASM)
    add_subdirectory(bindings/emscripten)
endif()

if(KOTLIN)
    add_subdirectory(bindings/kotlin)
endif()

if(SWIFT)
    add_subdirectory(bindings/swift)
endif()