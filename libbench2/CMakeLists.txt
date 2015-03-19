cmake_minimum_required(VERSION 2.6)

project(libbench2 C)

# default build type is Debug which means no optimization
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)

# if none of these options are enable, default is double precision.
option(BENCHFFT_LDOUBLE
  "Define to compile in long-double precision." OFF
)

option(BENCHFFT_QUAD
  "Define to compile in quad precision." OFF
)

option(BENCHFFT_SINGLE 
  "Define to compile in single precision." OFF
)

include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckSymbolExists)

if(MSVC OR MINGW)
  # use our aligned malloc/free for 32 bit Windows
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_definitions(-DWITH_OUR_MALLOC)
  endif()

  if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_USE_MATH_DEFINES)
  endif(MSVC)
else()
  # some systems need libm for some of the math functions to work
  check_library_exists(m cos "" HAVE_LIBM)
  if(HAVE_LIBM)
    list(APPEND CMAKE_REQUIRED_LIBRARIES m)
    list(APPEND LIBBENCH2_EXTRA_LIBRARIES m)
  endif(HAVE_LIBM)
endif(MSVC OR MINGW)

check_include_file(malloc.h HAVE_MALLOC_H)
check_include_file(stdarg.h HAVE_STDARG_H)
check_include_file(stddef.h HAVE_STDDEF_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(string.h HAVE_STRING_H)

check_include_file(sys/time.h HAVE_SYS_TIME_H)
if(HAVE_SYS_TIME_H)
  list(APPEND CMAKE_REQUIRED_INCLUDES sys/time.h)
endif()

check_include_file(unistd.h HAVE_UNISTD_H)

check_symbol_exists(cosl           math.h   HAVE_DECL_COSL)
check_symbol_exists(srand48        stdlib.h HAVE_DECL_SRAND48)
check_symbol_exists(drand48        stdlib.h HAVE_DECL_DRAND48)
check_symbol_exists(memalign       malloc.h HAVE_DECL_MEMALIGN)
check_symbol_exists(posix_memalign stdlib.h HAVE_DECL_POSIX_MEMALIGN)

check_function_exists(cosl           HAVE_COSL)
check_function_exists(drand48        HAVE_DRAND48)
check_function_exists(gettimeofday   HAVE_GETTIMEOFDAY)
check_function_exists(memalign       HAVE_MEMALIGN)
check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)

if(BENCHFFT_QUAD AND NOT HAVE_LIBQUADMATH)
  message(WARNING "Failed to enable compile in quad precision, using long-double precision instead.")
  set(BENCHFFT_QUAD OFF)
  set(BENCHFFT_LDOUBLE ON)
endif(BENCHFFT_QUAD AND NOT HAVE_LIBQUADMATH)

if(BENCHFFT_LDOUBLE AND NOT HAVE_COSL)
  message(WARNING "Failed to enable compile in long-double precision instead, using double precision instead.")
  set(BENCHFFT_LDOUBLE OFF)
endif(BENCHFFT_LDOUBLE AND NOT HAVE_COSL)

set(LIBBENCH2_SOURCES
  after-ccopy-from.c
  after-ccopy-to.c
  after-hccopy-from.c
  after-hccopy-to.c
  after-rcopy-from.c
  after-rcopy-to.c
  allocate.c
  aset.c
  bench-cost-postprocess.c
  bench-exit.c
  bench-main.c
  bench-user.h
  bench.h
  can-do.c
  caset.c
  dotens2.c
  info.c
  main.c
  mflops.c
  mp.c
  my-getopt.c
  my-getopt.h
  ovtpvt.c
  pow2.c
  problem.c
  report.c
  speed.c
  tensor.c
  timer.c
  useropt.c
  util.c
  verify-dft.c
  verify-lib.c
  verify-r2r.c
  verify-rdft2.c
  verify.c
  verify.h
  zero.c
)

# Generate the configure headers.
# (Place them in the build dir so we don't polute the source tree with generated files).
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/include/libbench2)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libbench2-config.h.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/include/libbench2/config.h)

add_library(libbench2_static
  ${LIBBENCH2_SOURCES}
)

target_link_libraries(libbench2_static
  ${LIBBENCH2_EXTRA_LIBRARIES}
)
