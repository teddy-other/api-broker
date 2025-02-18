#
#  Copyright 2024 CUBRID Corporation
# 
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
# 
#       http://www.apache.org/licenses/LICENSE-2.0
# 
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# 
#
message(STATUS "======== CMakeLists.txt")

cmake_minimum_required(VERSION 3.16)

function(with_unit_tests res) # checks for -DUNIT_TESTS=ON or -DUNIT_TEST_???=ON
  get_cmake_property(vars VARIABLES)
  foreach(var ${vars})
    if(var MATCHES "^(UNIT_TESTS|UNIT_TEST_.*)$")
      if(${${var}} STREQUAL "ON")
        set(${res} 1 PARENT_SCOPE)  #at least one unit test was requested
      endif()
    endif()
  endforeach()
endfunction(with_unit_tests)

with_unit_tests(AT_LEAST_ONE_UNIT_TEST)
message("    ==== with_unit_tests() = ${AT_LEAST_ONE_UNIT_TEST}")

#also searches for modules onto api-broker/cmake
# add other Find[LIBRARY].cmake scripts in api-broker/cmake
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, default to Debug")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (default Debug)")
endif()

project(API-BROKER)

#TODO: see why the lines below have no effect
#list(APPEND CMAKE_CXX_SOURCE_FILE_EXTENSIONS "c")
#list(REMOVE_ITEM CMAKE_C_SOURCE_FILE_EXTENSIONS "c")

if(UNIX)
  message(STATUS "  OS = UNIX")
elseif(WIN32)
  message(STATUS "  OS = WIN32")
else()
  message(FATAL_ERROR "OS not supported")
endif()

# options
if(UNIX)
  set(ENABLE_SYSTEMTAP_DEFAULT ON)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
else(UNIX)
  set(ENABLE_SYSTEMTAP_DEFAULT OFF)
endif(UNIX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
message(STATUS "  CMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# FIXME: linux 32bit build mode not working now
option(ENABLE_32BIT "Build for 32-bit banaries (on 64-bit platform)" OFF)
option(ENABLE_SYSTEMTAP "Enable dynamic tracing support using systemtap" ${ENABLE_SYSTEMTAP_DEFAULT})
option(USE_DUMA "Use Detect Unintended Memory Access library" OFF)
option(USE_API_BROKER_ENV "Use API BROKER environment variables" ON)

# Version info
# TODO: remove BUILD_NUMBER file and replace with VERSION file
if(EXISTS ${CMAKE_SOURCE_DIR}/VERSION)
  set(VERSION_FILE VERSION)
elseif(EXISTS ${CMAKE_SOURCE_DIR}/VERSION-DIST)
  set(VERSION_FILE VERSION-DIST)
else(EXISTS ${CMAKE_SOURCE_DIR}/VERSION)
  message(FATAL_ERROR "Could not find a VERSION file")
endif(EXISTS ${CMAKE_SOURCE_DIR}/VERSION)
message(STATUS "Get version information from ${VERSION_FILE}")
# Generate the same file in other directory to trigger cmake re-configure when the file changes
configure_file(${VERSION_FILE} ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${VERSION_FILE}.check_cache COPYONLY)
file(STRINGS ${VERSION_FILE} VERSION_STR)
if(VERSION_STR MATCHES "[0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9]+)?(-[a-zA-Z0-9_]+)?")
  string(REGEX MATCHALL "[0-9]+|-([A-Za-z0-9_]+)" VERSION_MATCHES ${VERSION_STR})
  list(LENGTH VERSION_MATCHES VERSION_MATCHES_LENGTH)
  list(GET VERSION_MATCHES 0 API_BROKER_MAJOR_VERSION)
  list(GET VERSION_MATCHES 1 API_BROKER_MINOR_VERSION)
  list(GET VERSION_MATCHES 2 API_BROKER_PATCH_VERSION)
else()
  message(FATAL_ERROR "Failed to parse a version string from ${VERSION_FILE} file")
endif()

if(VERSION_MATCHES_LENGTH GREATER 3)
  list(GET VERSION_MATCHES 3 API_BROKER_EXTRA_VERSION)
  if(VERSION_MATCHES_LENGTH GREATER 4)
    list(GET VERSION_MATCHES 4 API_BROKER_HASH_TAG)
  endif(VERSION_MATCHES_LENGTH GREATER 4)
else(VERSION_MATCHES_LENGTH GREATER 3)
  find_package(Git)
  if(EXISTS "${CMAKE_SOURCE_DIR}/.git" AND GIT_FOUND)
    set(API_BROKER_MAJOR_START_DATE "2019-12-12")
    execute_process(COMMAND ${GIT_EXECUTABLE} --version
    COMMAND awk "{ printf \"%s\", $3 }"
    OUTPUT_VARIABLE GIT_VERSION
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    if(GIT_VERSION VERSION_GREATER 1.7.6)
      execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count --after ${API_BROKER_MAJOR_START_DATE} HEAD
        COMMAND awk "{ printf \"%04d\", $1 }"
        OUTPUT_VARIABLE commit_count RESULT_VARIABLE git_result
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    else(GIT_VERSION VERSION_GREATER 1.7.6)
      # If you have trouble with execute_process on Windows, it is likely an issue caused by git installation.
      # <git-installation-directory>/bin has two files, bash.exe and sh.exe, that messes up CMake's execute_process
      #
      # There are two possible fixes for this issue:
      # 1. Add <git-installation-directory>/usr/bin to %PATH% environment variable
      # 2. Remove <git-installation-directory>/bin from %PATH% environment variable
      # You may prefer the first option if you want to use git command in command prompt.
      #
      # If both fixes fail, just replace execute_process(...) with set (commit_count 0). It is not a critical step to generate the project.
      execute_process(COMMAND ${GIT_EXECUTABLE} log --after ${API_BROKER_MAJOR_START_DATE} --oneline
        COMMAND wc -l
        COMMAND awk "{ printf \"%04d\", $1 }"
        OUTPUT_VARIABLE commit_count RESULT_VARIABLE git_result
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    endif(GIT_VERSION VERSION_GREATER 1.7.6)
    if(git_result)
      message(FATAL_ERROR "Could not get count information from Git")
    endif(git_result)
    set(API_BROKER_EXTRA_VERSION ${commit_count})
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
      OUTPUT_VARIABLE commit_hash RESULT_VARIABLE git_result
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    if(git_result)
      message(FATAL_ERROR "Could not get hash information from Git")
    endif(git_result)
    set(API_BROKER_HASH_TAG -${commit_hash})
    # Generate the same file in other directory to trigger cmake re-configure when the HEAD changes
    configure_file(".git/HEAD" ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HEAD.check_cache COPYONLY)
  else(EXISTS "${CMAKE_SOURCE_DIR}/.git" AND GIT_FOUND)
    set(API_BROKER_EXTRA_VERSION 0)
  endif(EXISTS "${CMAKE_SOURCE_DIR}/.git" AND GIT_FOUND)
endif(VERSION_MATCHES_LENGTH GREATER 3)
set(API_BROKER_VERSION ${API_BROKER_MAJOR_VERSION}.${API_BROKER_MINOR_VERSION}.${API_BROKER_PATCH_VERSION}.${API_BROKER_EXTRA_VERSION}${API_BROKER_HASH_TAG})

set(MAJOR_RELEASE_STRING ${API_BROKER_MAJOR_VERSION}.${API_BROKER_MINOR_VERSION})
set(PACKAGE_STRING "${PROJECT_NAME} ${MAJOR_RELEASE_STRING}")
set(PRODUCT_STRING "${MAJOR_RELEASE_STRING}")
# TODO: change to string type for RELEASE_STRING, BUILD_NUMBER, BUILD_OS
# TODO: remove #if defined (VERSION_STRING) in src/base/release_string.c
set(RELEASE_STRING ${API_BROKER_MAJOR_VERSION}.${API_BROKER_MINOR_VERSION}.${API_BROKER_PATCH_VERSION})
# BUILD_NUMBER (digital only version string) for legacy codes
set(BUILD_NUMBER ${API_BROKER_MAJOR_VERSION}.${API_BROKER_MINOR_VERSION}.${API_BROKER_PATCH_VERSION}.${API_BROKER_EXTRA_VERSION})
set(BUILD_OS ${CMAKE_SYSTEM_NAME})

# BUILD_TYPE
if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(BUILD_TYPE "debug")
elseif(CMAKE_BUILD_TYPE MATCHES "Profile")
  string(TOLOWER "${CMAKE_BUILD_TYPE} debug" BUILD_TYPE)
elseif(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
  set(BUILD_TYPE "release")
else(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(BUILD_TYPE "unknown")
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

if(MSVC)
  configure_file(cmake/version.rc.cmake version.rc)
  # TODO: remove version.rc files from win/*
endif(MSVC)

# Language setting
#enable_language(C CXX)

# System check
if(UNIX)
  if(CMAKE_COMPILER_IS_GNUCC)
    if(ENABLE_32BIT AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.3.4)
      message(SEND_ERROR "Cannot compile 32-bit program with gcc version (${CMAKE_CXX_COMPILER_VERSION})")
      message(FATAL_ERROR "4.3.4 or higher version is required. (ref. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=16185)")
    endif(ENABLE_32BIT AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.3.4)

    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4.7)
      message(SEND_ERROR "Cannot compile with gcc version ${CMAKE_CXX_COMPILER_VERSION}. Require at least 4.4.7.")
    endif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4.7)

    # C flags for both debug and release build
    set(CMAKE_C_COMMON_FLAGS "-ggdb -fno-omit-frame-pointer")
    # C++ flags for both debug and release build
    set(CMAKE_CXX_COMMON_FLAGS "${CMAKE_C_COMMON_FLAGS} -std=c++17")

    # C flags for debug build
    set(CMAKE_C_DEBUG_FLAGS "-Wall -fno-inline ${CMAKE_C_COMMON_FLAGS}")
    # C++ flags for debug build
    set(CMAKE_CXX_DEBUG_FLAGS "-Wall -fno-inline ${CMAKE_CXX_COMMON_FLAGS}")

    # C flags for release build
    set(CMAKE_C_RELEASE_FLAGS "-O2 -DNDEBUG -finline-functions ${CMAKE_C_COMMON_FLAGS}")
    # C++ flags for release build
    set(CMAKE_CXX_RELEASE_FLAGS "-O2 -DNDEBUG -finline-functions ${CMAKE_CXX_COMMON_FLAGS}")

    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_DEBUG_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_DEBUG_FLAGS}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_RELEASE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_RELEASE_FLAGS}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CMAKE_C_RELEASE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CMAKE_CXX_RELEASE_FLAGS}")

    if(WITH_SOURCES)
      set(DEBUG_PREFIX_MAP_FLAG "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}/src=${CMAKE_INSTALL_PREFIX}/src")

      set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_PREFIX_MAP_FLAG}")
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_PREFIX_MAP_FLAG}")
      set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${DEBUG_PREFIX_MAP_FLAG}")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${DEBUG_PREFIX_MAP_FLAG}")
      set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${DEBUG_PREFIX_MAP_FLAG}")
      set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${DEBUG_PREFIX_MAP_FLAG}")
    endif(WITH_SOURCES)

    # set has-style
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--hash-style=both -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--hash-style=both -static-libstdc++")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--hash-style=both -static-libstdc++")

  else(CMAKE_COMPILER_IS_GNUCC)
    message(FATAL_ERROR "We currently do not support ${CMAKE_CXX_COMPILER_ID} compiler")
  endif(CMAKE_COMPILER_IS_GNUCC)
else(UNIX)
  # turn off LinkIncremental option for release build

  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replaceIncFlag ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/INCREMENTAL:NO ${replaceIncFlag}" )

  string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replaceIncFlag ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})
  set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/INCREMENTAL:NO ${replaceIncFlag}" )

  #set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX /wd4820 /showIncludes" )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /wd4820 /std:c++17" )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3" )
  set(CMAKE_CXX_STANDARD 17)
endif(UNIX)

# check target platform
# Test 32/64 bits
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(TARGET_PLATFORM_BITS 64)
else()
  message(FATAL_ERROR "Target platform 32bit is no longer supported as of API BROKER 1.0")
endif()

message(STATUS "Build ${PROJECT_NAME} ${API_BROKER_VERSION} ${TARGET_PLATFORM_BITS}bit ${CMAKE_BUILD_TYPE} on ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")

# Build types
if(CMAKE_BUILD_TYPE MATCHES "Profile")
  set(CMAKE_CXX_FLAGS_PROFILE "-g -pg -std=c++17"
    CACHE STRING "Flags used by the c++ compiler during profile build." FORCE)
  set(CMAKE_C_FLAGS_PROFILE "-g -pg"
    CACHE STRING "Flags used by the c compiler during profile build." FORCE)

  mark_as_advanced(CMAKE_CXX_FLAGS_PROFILE CMAKE_C_FLAGS_PROFILE)
endif(CMAKE_BUILD_TYPE MATCHES "Profile")


# source directories
set(BASE_DIR                 ${CMAKE_SOURCE_DIR}/src/base)
set(BROKER_DIR               ${CMAKE_SOURCE_DIR}/src/broker)
set(S62_DIR                  ${CMAKE_SOURCE_DIR}/src/s62)
set(S62_LIB_NAME             "libtbgpp-c-api-shared.so")
get_filename_component(S62_LIB_DIR "../build/tbgpp-api/c-api" ABSOLUTE)
message("-- S62_LIB_DIR = ${S62_LIB_DIR}")

find_file(S62_API_SERVER_LIB
        ${S62_LIB_NAME}
        PATH ${S62_LIB_DIR}
        NO_DEFAULT_PATH
        REQUIRED)
message("-- S62_API_SERVER_LIB = ${S62_API_SERVER_LIB}")

if(S62_API_SERVER_LIB MATCHES "S62_API_SERVER_LIB-NOTFOUND")
message(FATAL_ERROR "S62_API_SERVER_LIB IS NOT FOUND PATH : ${S62_LIB_DIR}/${S62_LIB_NAME}")
endif(S62_API_SERVER_LIB MATCHES "S62_API_SERVER_LIB-NOTFOUND")


include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(
  include
  src/broker
  )

include(CheckFunctionExists)
check_function_exists(asprintf HAVE_ASPRINTF)
check_function_exists(vasprintf HAVE_VASPRINTF)
check_function_exists(basename HAVE_BASENAME)
check_function_exists(dirname HAVE_DIRNAME)
check_function_exists(ctime_r HAVE_CTIME_R)
check_function_exists(localtime_r HAVE_LOCALTIME_R)
check_function_exists(drand48_r HAVE_DRAND48_R)
check_function_exists(gethostbyname_r HAVE_GETHOSTBYNAME_R)
check_function_exists(getopt_long HAVE_GETOPT_LONG)
check_function_exists(open_memstream HAVE_OPEN_MEMSTREAM)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strlcpy HAVE_STRLCPY)

if(WIN32)
  set(NOMINMAX 1)
endif(WIN32)

include(CheckIncludeFile)
check_include_file(err.h HAVE_ERR_H)
check_include_file(getopt.h HAVE_GETOPT_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(libgen.h HAVE_LIBGEN_H)
check_include_file(limits.h HAVE_LIMITS_H)
if(NOT HAVE_LIMITS_H)
  set(PATH_MAX 512)
  set(NAME_MAX 255)
  set(LINE_MAX 2048)
endif(NOT HAVE_LIMITS_H)
if(WIN32)
  set(LINE_MAX 2048)
endif(WIN32)

check_include_file(memory.h HAVE_MEMORY_H)
check_include_file(nl_types.h HAVE_NL_TYPES_H)
check_include_file(regex.h HAVE_REGEX_H)
check_include_file(rpc/des_crypt.h HAVE_RPC_DES_CRYPT_H)
check_include_file(stdbool.h HAVE_STDBOOL_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(stddef.h HAVE_STDDEF_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(sys/param.h HAVE_SYS_PARAM_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)
if(HAVE_STDLIB_H AND HAVE_STDDEF_H)
  set(STDC_HEADERS 1)
endif(HAVE_STDLIB_H AND HAVE_STDDEF_H)

include(CheckTypeSize)
check_type_size(char SIZEOF_CHAR)
check_type_size(short SIZEOF_SHORT)
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size("long long" SIZEOF_LONG_LONG)
check_type_size("void *" SIZEOF_VOID_P)
check_type_size(byte_t BYTE_T)
check_type_size(int8_t INT8_T)
check_type_size(int16_t INT16_T)
check_type_size(int32_t INT32_T)
check_type_size(int64_t INT64_T)
check_type_size(intptr_t INTPTR_T)
check_type_size(uint8_t UINT8_T)
check_type_size(uint16_t UINT16_T)
check_type_size(uint32_t UINT32_T)
check_type_size(uint64_t UINT64_T)
check_type_size(uintptr_t UINTPTR_T)
check_type_size("off_t" SIZEOF_OFF_T)
if(NOT SIZEOF_OFF_T)
  set(off_t "long int")
endif()
check_type_size("size_t" SIZEOF_SIZE_T)
if(NOT SIZEOF_SIZE_T)
  set(size_t "unsinged int")
endif()
check_type_size("pid_t" SIZEOF_PID_T)
if(NOT SIZEOF_PID_T)
  # TODO: util_sa.c:79 remove typedef int pid_t
  if(NOT WIN32)
    set(pid_t "int")
  endif(NOT WIN32)
endif()
set(CMAKE_REQUIRED_FLAGS "-D_LARGEFILE64_SOURCE -finline-functions")
check_type_size("off64_t" SIZEOF_OFF64_T)
unset(CMAKE_REQUIRED_FLAGS)

include(CheckPrototypeDefinition)
if(HAVE_GETHOSTBYNAME_R)
  check_prototype_definition(gethostbyname_r
    "int gethostbyname_r(const char *name, struct hostent *hp, char *buf, size_t buflen, struct hostent **result, int *herr)"
    "0"
    "netdb.h"
    HAVE_GETHOSTBYNAME_R_GLIBC)
  check_prototype_definition(gethostbyname_r
    "struct hostent *gethostbyname_r(const char *name, struct hostent *hp, char *buf, size_t buflen, int *herr)"
    "NULL"
    "netdb.h"
    HAVE_GETHOSTBYNAME_R_SOLARIS)
  check_prototype_definition(gethostbyname_r
    "int gethostbyname_r(const char *name, struct hostent *hp, struct hostent_data *hdata)"
    "0"
    "netdb.h"
    HAVE_GETHOSTBYNAME_R_HOSTENT_DATA)
endif(HAVE_GETHOSTBYNAME_R)

include(CheckSymbolExists)
if(NOT HAVE_STDBOOL_H)
  check_symbol_exists( "_Bool" stdbool.h HAVE__BOOL)
endif(NOT HAVE_STDBOOL_H)

include(CheckCSourceCompiles)
check_c_source_compiles("#include <sys/time.h>\n#include <time.h>\nmain(){}" TIME_WITH_SYS_TIME)
check_c_source_compiles("#include <stdint.h>
int main() {
  uint64_t i, j, r;
  i = 100; j = 200; r = __sync_bool_compare_and_swap(&i, 100, j);
  if (r == 0 || i != 200) { return(1); }
  i = 100; j = 200; r = __sync_bool_compare_and_swap(&i, 101, j);
  if (r != 0 || i != 100) { return(1); }
  i = 100; j = 200; r = __sync_add_and_fetch(&i, j);
  if (r != 300 || i != 300) { return(1); }
  i = 100; r = __sync_lock_test_and_set(&i, 200);
  if (r != 100 || i != 200) { return(1); }
  return(0);
}"
  HAVE_GCC_ATOMIC_BUILTINS)

include(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)

if(UNIX)
  include(FindThreads)
endif(UNIX)


configure_file(cmake/config.h.cmake config.h)
# TODO: merge version.h into config.h ?
configure_file(cmake/version.h.cmake version.h)

add_definitions(-DSUPPORT_DEDUPLICATE_KEY_MODE)

# platform specifie configurations
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  # Definitions for system
  add_definitions(-DGCC -DLINUX -D_GNU_SOURCE -DI386 -DX86)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wwrite-strings -Wno-cast-qual -Wmissing-prototypes -Wredundant-decls -Wextra -Wno-unused -Wno-format-security -pthread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wwrite-strings -Wno-cast-qual -Wredundant-decls -Wextra -Wno-unused -Wno-unused-parameter -Wno-format-security -pthread")

  if(SIZEOF_OFF64_T)
    add_definitions( -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)
  endif(SIZEOF_OFF64_T)

  if(ENABLE_32BIT AND ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
    set(CMAKE_C_COMPILER_ARG1 "-m32 -march=i686")
    set(CMAKE_CXX_COMPILER_ARG1 "-m32 -march=i686")
    #set(CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
    #set(CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
  endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "AIX")
  # Definitions for system
  add_definitions(-DGCC -DAIX -D_GNU_SOURCE -DPPC -fsigned-char -D_THREAD_SAFE)

  if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
    add_definitions(-DAIX_PPC64 -D__WORDSIZE=64)
    set(CMAKE_C_COMPILER_ARG1 "-pthread -maix64")
    set(CMAKE_CXX_COMPILER_ARG1 "-pthread -maix64")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-bbigtoc -Wl,-brtl -Wl,-bnortllib -Wl,-bnosymbolic -Wl,-bnoautoexp -Wl,-bexpall -Wl,-bmaxdata:0x200000000 -Wl,-bnoipath" CACHE INTERNAL "" FORCE)
  else()
    add_definitions(-D_LARGE_FILES -DAIX_PPC32 -D__WORDSIZE=32)
    set(CMAKE_C_COMPILER_ARG1 "-pthread -maix32")
    set(CMAKE_C_COMPILER_ARG1 "-pthread -maix32")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-bbigtoc -Wl,-brtl -Wl,-bnortllib -Wl,-bnosymbolic -Wl,-bnoautoexp -Wl,-bexpall -Wl,-bmaxdata:0x80000000 -Wl,-bnoipath" CACHE INTERNAL "" FORCE)
  endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DWIN -DWINDOWS -D_CRT_SECURE_NO_WARNINGS /wd4274 /wd4996)
  string(REPLACE "/DWIN32" "/D_WIN64" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  if(MSVC_VERSION GREATER 1400)
    # experimental option (it can lead the build system to a halt or creating a bad object files)
    set(PARALLEL_JOBS "0" CACHE STRING "Specifies the number of jots to build parallel. 0 means OFF")
    if(PARALLEL_JOBS GREATER $ENV{NUMBER_OF_PROCESSORS})
      add_definitions(/MP)
    elseif(PARALLEL_JOBS GREATER 0)
      add_definitions(/MP${PARALLEL_JOBS})
    endif(PARALLEL_JOBS GREATER $ENV{NUMBER_OF_PROCESSORS})
  endif(MSVC_VERSION GREATER 1400)
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")

# common definitions
set(COMMON_DEFS SYSV MAXPATHLEN=1024 -D_REENTRANT)

# For systemtap options
if(ENABLE_SYSTEMTAP)
  find_program(DTRACE dtrace)
  if(NOT DTRACE)
    message(FATAL_ERROR "The dtrace tool was not found. check PATH or install dtrace")
  endif(NOT DTRACE)
  mark_as_advanced(DTRACE)
  execute_process(COMMAND ${DTRACE} -V OUTPUT_VARIABLE dtrace_version)
  if(dtrace_version MATCHES "Sun D" OR dtrace_version MATCHES "Oracle D")
    message(WARNING "Sun or Oracle D is not supported")
    message(FATAL_ERROR "Please use sourceware.org's systemtap [https://sourceware.org/systemtap/]")
  endif(dtrace_version MATCHES "Sun D" OR dtrace_version MATCHES "Oracle D")
  check_include_file(sys/sdt.h HAVE_SYS_SDT_H)
  if(NOT HAVE_SYS_SDT_H)
    message(FATAL_ERROR "The systemtap support needs sys/sdt.h header")
  endif(NOT HAVE_SYS_SDT_H)

  set(PROBES_SOURCE ${CMAKE_SOURCE_DIR}/contrib/systemtap/probes.d)
  set(PROBES_HEADER ${CMAKE_BINARY_DIR}/probes.h)
  set(PROBES_OBJECT ${CMAKE_BINARY_DIR}/probes.o)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/probes.h ${PROBES_OBJECT}
    COMMAND ${DTRACE} -C -h -s ${PROBES_SOURCE} -o ${PROBES_HEADER}
    COMMAND ${DTRACE} -C -G -s ${PROBES_SOURCE} -o ${PROBES_OBJECT}
    DEPENDS ${PROBES_SOURCE}
    )
  add_custom_target(gen_probes DEPENDS ${PROBES_SOURCE} ${PROBES_HEADER} ${PROBES_OBJECT})
  set_target_properties(gen_probes PROPERTIES LINKER_LANGUAGE C)
endif(ENABLE_SYSTEMTAP)


# For duma option
if(USE_DUMA)
  find_library(DUMA_LIBRARIES duma)
  if(NOT DUMA_LIBRARIES)
    message(FATAL_ERROR "Cannot find DUMA library")
  endif(NOT DUMA_LIBRARIES)
  message(STATUS "Use DUMA library")
endif(USE_DUMA)


# For API Broker environment variable option
if(USE_API_BROKER_ENV)
  message(STATUS "Default install prefix: ${CMAKE_INSTALL_PREFIX}")
  set(API_BROKER_PREFIXDIR .)
  set(API_BROKER_BINDIR bin)
  set(API_BROKER_LIBDIR lib)
  set(API_BROKER_SRCDIR src)
  set(API_BROKER_INCLUDEDIR include)
  set(API_BROKER_VARDIR var)
  set(API_BROKER_DATADIR share)
  set(API_BROKER_CONFDIR conf)
  set(API_BROKER_DATABASE_CONFDIR data)
  set(API_BROKER_JAVADIR java)
  set(API_BROKER_LOCALEDIR msg)
  set(API_BROKER_LOGDIR log)
  set(API_BROKER_TMPDIR tmp)
  set(API_BROKER_SHLOCALESDIR locales)
  set(API_BROKER_TZDIR timezones)
  set(API_BROKER_DEMODIR demo)
  set(API_BROKER_COMPATDIR compat)
  set(API_BROKER_DATABASES databases)
  set(API_BROKER_SOCK API_BROKER_SOCK)
else(USE_API_BROKER_ENV)
  message(WARNING "This build does not use API BROKER environment variables")
  list(APPEND COMMON_DEFS DO_NOT_USE_APIBROKERENV)

  set(API_BROKER_PREFIXDIR ${CMAKE_INSTALL_PREFIX} CACHE STRING "The prefix directory for API BROKER (default: ${CMAKE_INSTALL_PREFIX}")
  list(APPEND DIR_DEFS API_BROKER_PREFIXDIR="${API_BROKER_PREFIXDIR}")
  set(API_BROKER_BINDIR bin CACHE STRING "The directory for binary files")
  if(IS_ABSOLUTE ${API_BROKER_BINDIR})
    list(APPEND DIR_DEFS API_BROKER_BINDIR="${API_BROKER_BINDIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_BINDIR="${API_BROKER_PREFIXDIR}/${API_BROKER_BINDIR}")
  endif()
  set(API_BROKER_LIBDIR lib CACHE STRING "The directory for library files")
  if(IS_ABSOLUTE ${API_BROKER_LIBDIR})
    list(APPEND DIR_DEFS API_BROKER_LIBDIR="${API_BROKER_LIBDIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_LIBDIR="${API_BROKER_PREFIXDIR}/${API_BROKER_LIBDIR}")
  endif()
  set(API_BROKER_SRCDIR src CACHE STRING "The directory for source files")
  if(IS_ABSOLUTE ${API_BROKER_SRCDIR})
    list(APPEND DIR_DEFS API_BROKER_SRCDIR="${API_BROKER_SRCDIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_SRCDIR="${API_BROKER_PREFIXDIR}/${API_BROKER_SRCDIR}")
  endif()
  set(API_BROKER_VARDIR var CACHE STRING "The directory for variable data files")
  if(IS_ABSOLUTE ${API_BROKER_VARDIR})
    list(APPEND DIR_DEFS API_BROKER_VARDIR="${API_BROKER_VARDIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_VARDIR="${API_BROKER_PREFIXDIR}/${API_BROKER_VARDIR}")
  endif()
  set(API_BROKER_DATADIR share CACHE STRING "The directory for shared data files")
  if(IS_ABSOLUTE ${API_BROKER_DATADIR})
    list(APPEND DIR_DEFS API_BROKER_DATADIR="${API_BROKER_DATADIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_DATADIR="${API_BROKER_PREFIXDIR}/${API_BROKER_DATADIR}")
  endif()
  set(API_BROKER_CONFDIR conf CACHE STRING "The directory for configuration files")
  if(IS_ABSOLUTE ${API_BROKER_CONFDIR})
    list(APPEND DIR_DEFS API_BROKER_CONFDIR="${API_BROKER_CONFDIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_CONFDIR="${API_BROKER_PREFIXDIR}/${API_BROKER_CONFDIR}")
  endif()
  if(IS_ABSOLUTE ${API_BROKER_DATABASE_CONFDIR})
    list(APPEND DIR_DEFS API_BROKER_DATABASE_CONFDIR="${API_BROKER_DATABASE_CONFDIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_DATABASE_CONFDIR="${API_BROKER_PREFIXDIR}/${API_BROKER_DATABASE_CONFDIR}")
  endif()
  set(API_BROKER_JAVADIR java CACHE STRING "The directory for JSP file")
  if(IS_ABSOLUTE ${API_BROKER_JAVADIR})
    list(APPEND DIR_DEFS API_BROKER_JAVADIR="${API_BROKER_JAVADIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_JAVADIR="${API_BROKER_PREFIXDIR}/${API_BROKER_JAVADIR}")
  endif()
  set(API_BROKER_LOCALEDIR msg CACHE STRING "The directory for message files")
  if(IS_ABSOLUTE ${API_BROKER_LOCALEDIR})
    list(APPEND DIR_DEFS API_BROKER_LOCALEDIR="${API_BROKER_LOCALEDIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_LOCALEDIR="${API_BROKER_PREFIXDIR}/${API_BROKER_LOCALEDIR}")
  endif()
  set(API_BROKER_LOGDIR log CACHE STRING "The directory for log files")
  if(IS_ABSOLUTE ${API_BROKER_LOGDIR})
    list(APPEND DIR_DEFS API_BROKER_LOGDIR="${API_BROKER_LOGDIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_LOGDIR="${API_BROKER_PREFIXDIR}/${API_BROKER_LOGDIR}")
  endif()
  set(API_BROKER_TMPDIR tmp CACHE STRING "The directory for temporary files")
  if(IS_ABSOLUTE ${API_BROKER_TMPDIR})
    list(APPEND DIR_DEFS API_BROKER_TMPDIR="${API_BROKER_TMPDIR}")
  else()
    list(APPEND DIR_DEFS API_BROKER_TMPDIR="${API_BROKER_PREFIXDIR}/${API_BROKER_TMPDIR}")
  endif()
  # directories for install
  set(API_BROKER_INCLUDEDIR include CACHE STRING "The directory for header files")
  set(API_BROKER_SHLOCALESDIR locales CACHE STRING "The directory for locale files")
  set(API_BROKER_TZDIR timezones CACHE STRING "The directory for timezone files")
  set(API_BROKER_DEMODIR demo CACHE STRING "The directory for demo data files")
  set(API_BROKER_COMPATDIR compat CACHE STRING "The directory for compatible files")
endif(USE_API_BROKER_ENV)

# For JVM libraries
if(WIN32)
    set(JVM_LIBS ${CMAKE_SOURCE_DIR}/win/3rdparty/lib64/jvm.lib)
  list(APPEND JVM_LIBS -DELAYLOAD:jvm.dll)
endif(WIN32)

# include subdirectories
add_subdirectory(broker)
add_subdirectory(conf)
add_subdirectory(data)
add_subdirectory(contrib)

# create empty directories
install(DIRECTORY DESTINATION ${API_BROKER_LOGDIR})
install(DIRECTORY DESTINATION ${API_BROKER_VARDIR})
install(DIRECTORY DESTINATION ${API_BROKER_TMPDIR})
install(DIRECTORY DESTINATION "${API_BROKER_VARDIR}/${API_BROKER_SOCK}")
if(USE_API_BROKER_ENV)
  install(FILES
#    ${CMAKE_SOURCE_DIR}/COPYING
#    ${CMAKE_SOURCE_DIR}/CREDITS
#    ${CMAKE_SOURCE_DIR}/README
    ${CMAKE_SOURCE_DIR}/contrib/readme/README_TAR_INSTALL
    DESTINATION ${API_BROKER_PREFIXDIR})
  install(FILES
    ${CMAKE_SOURCE_DIR}/contrib/readme/README_SOCK.txt
    DESTINATION ${API_BROKER_VARDIR}/${API_BROKER_SOCK}
    RENAME README.txt)
  install(FILES
    ${CMAKE_SOURCE_DIR}/contrib/readme/README_TMP.txt
    DESTINATION ${API_BROKER_TMPDIR}
    RENAME README.txt)
endif(USE_API_BROKER_ENV)

if(WITH_SOURCES)
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/ DESTINATION ${API_BROKER_SRCDIR})
endif(WITH_SOURCES)

# for packaging
if(UNIX)
  set(CPACK_GENERATOR "STGZ;TGZ")
  set(CPACK_SOURCE_GENERATOR "TGZ")
else(UNIX)
  set(CPACK_GENERATOR "ZIP")
endif(UNIX)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "API BROKER Open Source ")
set(CPACK_PACKAGE_VENDOR "CUBRID Corporation")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
# for WiX (txt file extension is required)
configure_file(COPYING LICENSE.txt)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_BINARY_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_VERSION_MAJOR "${API_BROKER_MAJOR_VERSION}")
set(CPACK_PACKAGE_VERSION_MINOR "${API_BROKER_MINOR_VERSION}")
set(CPACK_PACKAGE_VERSION_PATCH "${API_BROKER_PATCH_VERSION}")
set(CPACK_PACKAGE_VERSION "${API_BROKER_VERSION}")
if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(API_BROKER_PACKAGE_FILE_SUFFIX "-debug")
elseif(CMAKE_BUILD_TYPE MATCHES "Profile")
  set(API_BROKER_PACKAGE_FILE_SUFFIX "-profile")
else(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(API_BROKER_PACKAGE_FILE_SUFFIX "")
endif(CMAKE_BUILD_TYPE MATCHES "Debug")
if(WIN32)
  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}-x64-${API_BROKER_VERSION}${API_BROKER_PACKAGE_FILE_SUFFIX}")
else(WIN32)
  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${API_BROKER_VERSION}-${CMAKE_SYSTEM_NAME}.${CMAKE_HOST_SYSTEM_PROCESSOR}${API_BROKER_PACKAGE_FILE_SUFFIX}")
endif(WIN32)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${API_BROKER_VERSION}-src")

set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE ON)

configure_file(cmake/CPackOptions.cmake.in CPackOptions.cmake)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackOptions.cmake")

include(CPack)
