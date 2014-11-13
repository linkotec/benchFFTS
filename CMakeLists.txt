cmake_minimum_required(VERSION 2.8)

project(benchFFTS C)

# default build type is Debug which means no optimization
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)

# control the precision of libbench2
set(BENCHFFT_LDOUBLE OFF CACHE BOOL "Disable compile in long-double precision.")
set(BENCHFFT_QUAD OFF CACHE BOOL "Disable compile in quad precision.")
set(BENCHFFT_SINGLE ON CACHE BOOL "Enable compile in single precision.")

add_subdirectory(libbench2)
add_subdirectory(ffts EXCLUDE_FROM_ALL)

include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/libbench2/include/libbench2)
include_directories(libbench2)
include_directories(ffts/include)

set(BENCHFFTS_SOURCES
  bench.c
  #fftw-bench.c
  #fftw-bench.h
  #hook.c
)

add_executable(bench_ffts
  ${BENCHFFTS_SOURCES}
)

target_link_libraries(bench_ffts
  ffts_static
  libbench2_static
)