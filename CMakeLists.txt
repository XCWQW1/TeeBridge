cmake_minimum_required(VERSION 3.14)
project(TeeBridge)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/engine/shared
        ${PROJECT_BINARY_DIR}/src
        ${PROJECT_BINARY_DIR}/src/game
)


########################################################################
# COMMON FUNCTIONS
########################################################################

function(set_glob VAR GLOBBING EXTS DIRECTORY) # ...
    set(GLOBS)
    foreach(ext ${EXTS})
        list(APPEND GLOBS "${DIRECTORY}/*.${ext}")
    endforeach()
    file(${GLOBBING} GLOB_RESULT ${GLOBS})
    list(SORT GLOB_RESULT)
    set(FILES)
    foreach(file ${ARGN})
        list(APPEND FILES "${PROJECT_SOURCE_DIR}/${DIRECTORY}/${file}")
    endforeach()

    if(NOT FILES STREQUAL GLOB_RESULT)
        message(AUTHOR_WARNING "${VAR} does not contain every file from directory ${DIRECTORY}")
        set(LIST_BUT_NOT_GLOB)
        foreach(file ${FILES})
            if(NOT file IN_LIST GLOB_RESULT)
                list(APPEND LIST_BUT_NOT_GLOB ${file})
            endif()
        endforeach()
        if(LIST_BUT_NOT_GLOB)
            message(AUTHOR_WARNING "Entries only present in ${VAR}: ${LIST_BUT_NOT_GLOB}")
        endif()
        set(GLOB_BUT_NOT_LIST)
        foreach(file ${GLOB_RESULT})
            if(NOT file IN_LIST FILES)
                list(APPEND GLOB_BUT_NOT_LIST ${file})
            endif()
        endforeach()
        if(GLOB_BUT_NOT_LIST)
            message(AUTHOR_WARNING "Entries only present in ${DIRECTORY}: ${GLOB_BUT_NOT_LIST}")
        endif()
        if(NOT LIST_BUT_NOT_GLOB AND NOT GLOB_BUT_NOT_LIST)
            message(AUTHOR_WARNING "${VAR} is not alphabetically sorted")
        endif()
    endif()

    set(${VAR} ${FILES} PARENT_SCOPE)
endfunction()

set(CHECKSUM_SRC)

function(set_own_rpath TARGET)
    if(NOT TARGET_OS STREQUAL "windows" AND NOT TARGET_OS STREQUAL "mac")
        set_property(TARGET ${TARGET} PROPERTY BUILD_RPATH "$ORIGIN")
        set_property(TARGET ${TARGET} PROPERTY INSTALL_RPATH "$ORIGIN/../lib/ddnet")
    endif()
endfunction()

function(set_src VAR GLOBBING DIRECTORY) # ...
    set_glob(${VAR} ${GLOBBING} "c;cpp;h" ${DIRECTORY} ${ARGN})
    set(${VAR} ${${VAR}} PARENT_SCOPE)
    set(CHECKSUM_SRC ${CHECKSUM_SRC} ${${VAR}} PARENT_SCOPE)
endfunction()

message(STATUS "******** ${CMAKE_PROJECT_NAME} ********")
set(TARGET "Target OS: ${TARGET_OS} ${CMAKE_SYSTEM_PROCESSOR}")
if(TARGET_OS STREQUAL "mac")
    set(TARGET "${TARGET} (SDK: ${CMAKE_OSX_SYSROOT}, architectures: ${CMAKE_OSX_ARCHITECTURES})")
endif()
message(STATUS ${TARGET})
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Rust version: ${RUST_VERSION_STRING}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

message(STATUS "Dependencies:")
function(show_dependency_status OUTPUT_NAME NAME)
    if(${NAME}_FOUND)
        if(${NAME}_BUNDLED)
            message(STATUS " * ${OUTPUT_NAME} not found (using bundled version)")
        else()
            message(STATUS " * ${OUTPUT_NAME} found")
        endif()
    else()
        message(STATUS " * ${OUTPUT_NAME} not found")
    endif()
endfunction()

find_package(CURL REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(RUSTC_EXECUTABLE rustc)
find_program(CARGO_EXECUTABLE cargo)
find_package(ZLIB)

set(RUST_CARGO ${CARGO_EXECUTABLE})

show_dependency_status("Curl" CURL)
show_dependency_status("Python3" Python3)
show_dependency_status("Zlib" ZLIB)

if(NOT(Python3_FOUND))
    message(SEND_ERROR "You must install Python to compile ${CMAKE_PROJECT_NAME}")
endif()
if(NOT(CURL_FOUND))
    message(SEND_ERROR "You must install Curl to compile ${CMAKE_PROJECT_NAME}")
endif()

########################################################################
# DEPENDENCY COMPILATION
########################################################################

set_src(DEP_JSON_SRC GLOB src/engine/external/json-parser json.c json.h)
add_library(json EXCLUDE_FROM_ALL OBJECT ${DEP_JSON_SRC})

set_src(DEP_MD5_SRC GLOB src/engine/external/md5 md5.c md5.h)
add_library(md5 EXCLUDE_FROM_ALL OBJECT ${DEP_MD5_SRC})

list(APPEND TARGETS_DEP json md5)
set(DEP_JSON $<TARGET_OBJECTS:json>)
set(DEP_MD5)
if(NOT CRYPTO_FOUND)
    set(DEP_MD5 $<TARGET_OBJECTS:md5>)
endif()

########################################################################
# 协议生成逻辑
########################################################################
function(generate_source output_file script_parameter)
    add_custom_command(OUTPUT ${output_file}
            COMMAND ${Python3_EXECUTABLE} datasrc/compile.py ${script_parameter}
            > "${PROJECT_BINARY_DIR}/${output_file}"
            DEPENDS
            datasrc/compile.py
            datasrc/content.py
            datasrc/datatypes.py
            datasrc/network.py
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endfunction()

function(generate_source7 output_file script_parameter)
    add_custom_command(OUTPUT ${output_file}
            COMMAND ${Python3_EXECUTABLE} -m datasrc.seven.compile ${script_parameter}
            > "${PROJECT_BINARY_DIR}/${output_file}"
            DEPENDS
            datasrc/seven/compile.py
            datasrc/seven/content.py
            datasrc/seven/datatypes.py
            datasrc/seven/network.py
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endfunction()

function(generate_maps output_file script_parameter)
    add_custom_command(OUTPUT ${output_file}
            COMMAND ${Python3_EXECUTABLE} datasrc/crosscompile.py ${script_parameter}
            > "${PROJECT_BINARY_DIR}/${output_file}"
            DEPENDS
            datasrc/crosscompile.py
            datasrc/compile.py
            datasrc/content.py
            datasrc/datatypes.py
            datasrc/network.py
            datasrc/seven/compile.py
            datasrc/seven/content.py
            datasrc/seven/datatypes.py
            datasrc/seven/network.py
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endfunction()

set(PROTOCOL_GEN_DIR "${PROJECT_BINARY_DIR}/src/game/generated")
file(MAKE_DIRECTORY "${PROTOCOL_GEN_DIR}")

generate_source("src/game/generated/data_types.h" "content_types_header")
generate_source("src/game/generated/protocol.cpp" "network_source")
generate_source("src/game/generated/protocol.h" "network_header")
generate_source("src/game/generated/server_data.cpp" "server_content_source")
generate_source("src/game/generated/server_data.h" "server_content_header")

generate_source7("src/game/generated/protocol7.cpp" "network_source")
generate_source7("src/game/generated/protocol7.h" "network_header")

generate_maps("src/game/generated/protocolglue.h" "map_header")
generate_maps("src/game/generated/protocolglue.cpp" "map_source")

add_custom_command(OUTPUT "src/game/generated/wordlist.h"
        COMMAND ${Python3_EXECUTABLE} scripts/wordlist.py > ${PROJECT_BINARY_DIR}/src/game/generated/wordlist.h
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        DEPENDS
        scripts/wordlist.py
)

include_directories(
        ${PROTOCOL_GEN_DIR}
        ${PROJECT_BINARY_DIR}/src/game/generated
)

add_custom_target(protocol_generated_files
        DEPENDS
        "${PROTOCOL_GEN_DIR}/protocol.cpp"
        "${PROTOCOL_GEN_DIR}/protocol.h"
        "${PROTOCOL_GEN_DIR}/data_types.h"
        "${PROTOCOL_GEN_DIR}/server_data.cpp"
        "${PROTOCOL_GEN_DIR}/server_data.h"
        "${PROTOCOL_GEN_DIR}/protocol7.cpp"
        "${PROTOCOL_GEN_DIR}/protocol7.h"
        "${PROTOCOL_GEN_DIR}/protocolglue.h"
        "${PROTOCOL_GEN_DIR}/protocolglue.cpp"
)

########################################################################
# RUST
########################################################################

set_glob(RUST_BASE GLOB_RECURSE "rs;toml" src/base
        Cargo.toml
        color.rs
        lib.rs
        rust.rs
)

set_glob(RUST_ENGINE_INTERFACE GLOB "rs;toml" src/engine
        Cargo.toml
        console.rs
        lib.rs
)

set_glob(RUST_ENGINE_SHARED GLOB_RECURSE "rs;toml" src/engine/shared
        Cargo.toml
        build.rs
        config.rs
        lib.rs
        rust_version.rs
)

set_src(RUST_BRIDGE_SHARED GLOB_RECURSE src/rust-bridge
        cpp/console.cpp
        cpp/console.h
        engine/shared/rust_version.cpp
        engine/shared/rust_version.h
)

add_library(rust-bridge-shared EXCLUDE_FROM_ALL OBJECT ${RUST_BRIDGE_SHARED})
list(APPEND TARGETS_OWN rust-bridge-shared)

set(CARGO_BUILD_DIR "")
set(CARGO_BUILD ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${PROJECT_BINARY_DIR} DDNET_TEST_NO_LINK=1 ${RUST_CARGO} build --manifest-path "${PROJECT_SOURCE_DIR}/Cargo.toml")
set(CARGO_TEST ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${PROJECT_BINARY_DIR} ${RUST_CARGO} test)

if(MSVC)
    list(INSERT CARGO_BUILD 0 ${CMAKE_COMMAND} -E env $<$<CONFIG:Debug>:CFLAGS=/MTd> $<$<CONFIG:Debug>:CXXFLAGS=/MTd>)
    list(INSERT CARGO_TEST 0 ${CMAKE_COMMAND} -E env RUSTFLAGS=-Ctarget-feature=+crt-static)
endif()
if(RUST_NIGHTLY)
    list(APPEND CARGO_BUILD -Z build-std=std,panic_abort)
endif()
if(NOT CMAKE_OSX_ARCHITECTURES AND (DEFINED CMAKE_RUST_COMPILER_TARGET OR RUST_NIGHTLY))
    if(DEFINED CMAKE_RUST_COMPILER_TARGET)
        list(APPEND CARGO_TEST --target ${CMAKE_RUST_COMPILER_TARGET})
        set(RUST_TARGET ${CMAKE_RUST_COMPILER_TARGET})
    else()
        set(RUST_TARGET ${RUST_TARGET_HOST})
    endif()
    list(APPEND CARGO_BUILD --target ${RUST_TARGET})
    set(CARGO_BUILD_DIR "${RUST_TARGET}/")
endif()
set(CARGO_BUILD_DIR_DEBUG "${CARGO_BUILD_DIR}debug")
set(CARGO_BUILD_DIR_RELEASE "${CARGO_BUILD_DIR}release")

# We can't check this directly; it is a cmake property, not a variable
get_cmake_property(MULTI_CONFIG_BUILD GENERATOR_IS_MULTI_CONFIG)
if(MULTI_CONFIG_BUILD)
    if(CMAKE_VERSION VERSION_LESS 3.20)
        message(SEND_ERROR "Multi-config generators only supported from CMake 3.20 and up")
    else()
        set(CARGO_BUILD_DIR "${CARGO_BUILD_DIR}$<$<CONFIG:Debug>:debug>$<$<NOT:$<CONFIG:Debug>>:release>")
    endif()
else()
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        set(CARGO_BUILD_DIR "${CARGO_BUILD_DIR_DEBUG}")
    else()
        set(CARGO_BUILD_DIR "${CARGO_BUILD_DIR_RELEASE}")
    endif()
endif()
list(APPEND CARGO_BUILD $<$<NOT:$<CONFIG:Debug>>:--release>)

if(CMAKE_OSX_ARCHITECTURES)
    set(RUST_OSX_ARCHITECTURES)
    foreach(arch ${CMAKE_OSX_ARCHITECTURES})
        if(${arch} STREQUAL arm64)
            list(APPEND RUST_OSX_ARCHITECTURES aarch64-apple-darwin)
        elseif(${arch} STREQUAL x86_64)
            list(APPEND RUST_OSX_ARCHITECTURES x86_64-apple-darwin)
        else()
            message(SEND_ERROR "CMAKE_OSX_ARCHITECTURES' architecture ${arch} unknown, can't build Rust code (known: arm64, x86_64)")
        endif()
    endforeach()
endif()

set(RUST_SRC
        ${RUST_BASE}
        ${RUST_ENGINE_INTERFACE}
        ${RUST_ENGINE_SHARED}
        Cargo.toml
        Cargo.lock
)
set(RUST_TARGETS engine_shared)
if(NOT CMAKE_OSX_ARCHITECTURES)
    set(RUST_OUTPUTS)
    foreach(rust_target ${RUST_TARGETS})
        set(LIBRARY_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}ddnet_${rust_target}${CMAKE_STATIC_LIBRARY_SUFFIX}")
        add_library(rust_${rust_target} STATIC IMPORTED GLOBAL)
        add_custom_target(rust_${rust_target}_target DEPENDS "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}")
        add_dependencies(rust_${rust_target} rust_${rust_target}_target)
        set_target_properties(rust_${rust_target} PROPERTIES
                IMPORTED_LOCATION "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR_RELEASE}/${LIBRARY_NAME}"
                IMPORTED_LOCATION_DEBUG "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR_DEBUG}/${LIBRARY_NAME}"
        )
        list(APPEND RUST_OUTPUTS "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}")
    endforeach()
    add_custom_command(
            OUTPUT ${RUST_OUTPUTS}
            COMMAND ${CARGO_BUILD}
            WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
            USES_TERMINAL
            DEPENDS ${RUST_SRC}
    )
else()
    foreach(rust_target ${RUST_TARGETS})
        set(LIBRARY_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}ddnet_${rust_target}${CMAKE_STATIC_LIBRARY_SUFFIX}")
        add_library(rust_${rust_target} STATIC IMPORTED GLOBAL)
        set_target_properties(rust_${rust_target} PROPERTIES
                IMPORTED_LOCATION "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR_RELEASE}/${LIBRARY_NAME}"
                IMPORTED_LOCATION_DEBUG "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR_DEBUG}/${LIBRARY_NAME}"
        )
        add_custom_target(rust_${rust_target}_target DEPENDS "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}")
        add_dependencies(rust_${rust_target} rust_${rust_target}_target)
        set(ARCH_LIBRARIES)
        foreach(arch ${RUST_OSX_ARCHITECTURES})
            list(APPEND ARCH_LIBRARIES "${PROJECT_BINARY_DIR}/${arch}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}")
        endforeach()
        add_custom_command(
                OUTPUT "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}"
                COMMAND lipo ${ARCH_LIBRARIES} -create -output "${PROJECT_BINARY_DIR}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}"
                DEPENDS ${ARCH_LIBRARIES}
        )
    endforeach()
    foreach(arch ${RUST_OSX_ARCHITECTURES})
        set(RUST_OUTPUTS)
        foreach(rust_target ${RUST_TARGETS})
            set(LIBRARY_NAME "${CMAKE_STATIC_LIBRARY_PREFIX}ddnet_${rust_target}${CMAKE_STATIC_LIBRARY_SUFFIX}")
            list(APPEND RUST_OUTPUTS "${PROJECT_BINARY_DIR}/${arch}/${CARGO_BUILD_DIR}/${LIBRARY_NAME}")
        endforeach()
        add_custom_command(
                OUTPUT ${RUST_OUTPUTS}
                COMMAND ${CARGO_BUILD} --target=${arch}
                WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
                USES_TERMINAL
                DEPENDS ${RUST_SRC}
        )
    endforeach()
endif()

########################################################################
# 收集核心网络层源文件
########################################################################
file(GLOB_RECURSE CORE_SOURCES
        # 核心基础库
        src/base/*.cpp
        src/base/*.h
        src/base/tl/*.cpp
        src/base/tl/*.h
        src/base/unicode/*.cpp
        src/base/unicode/*.h

        src/engine/shared/*.cpp
        src/engine/shared/*.h
        src/engine/server/*.cpp

        src/game/*.cpp
        src/game/*.h

        src/proxy/*.cpp
        src/proxy/*.h

        src/main.cpp

        ${PROTOCOL_GEN_DIR}/protocol.cpp
        ${PROTOCOL_GEN_DIR}/protocol.h
        ${PROTOCOL_GEN_DIR}/protocol7.cpp
        ${PROTOCOL_GEN_DIR}/protocol7.h
        ${PROTOCOL_GEN_DIR}/server_data.cpp
        ${PROTOCOL_GEN_DIR}/server_data.h
        ${PROTOCOL_GEN_DIR}/protocolglue.cpp
        ${PROTOCOL_GEN_DIR}/protocolglue.h
)

message(STATUS "Collected source files")

add_executable(TeeBridge ${CORE_SOURCES})
target_compile_options(TeeBridge PRIVATE -fconcepts)

add_dependencies(TeeBridge protocol_generated_files)
target_link_libraries(TeeBridge PRIVATE
        $<TARGET_OBJECTS:json>
        $<TARGET_OBJECTS:md5>
        ZLIB::ZLIB
        CURL::libcurl
        rust_engine_shared
)