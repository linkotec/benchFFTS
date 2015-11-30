cmake_minimum_required(VERSION 2.8)

project(bench_ffts C)

# default build type is Debug which means no optimization
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)

option(BENCHFFTS_ENABLE_SHARED
  "Enable building a shared FFTS library." OFF
)

# control the precision of libbench2
set(BENCHFFT_LDOUBLE OFF CACHE BOOL "Disable compile in long-double precision.")
set(BENCHFFT_QUAD OFF CACHE BOOL "Disable compile in quad precision.")
set(BENCHFFT_SINGLE ON CACHE BOOL "Enable compile in single precision.")

add_subdirectory(libbench2)

if(BENCHFFTS_ENABLE_SHARED)
  set(ENABLE_SHARED ON CACHE BOOL "Enable shared library.")
  set(ENABLE_STATIC OFF CACHE BOOL "Disable static library.")
endif(BENCHFFTS_ENABLE_SHARED)

add_subdirectory(ffts EXCLUDE_FROM_ALL)

include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/libbench2/include/libbench2)
include_directories(libbench2)
include_directories(ffts/include)

add_executable(bench_ffts
  bench.c
)

# if BENCHFFTS_ENABLE_SHARED then "ffts" is alias for "ffts_shared", otherwise "ffts_static"
target_link_libraries(bench_ffts
  ffts
  libbench2_static
)