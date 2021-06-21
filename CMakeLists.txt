cmake_minimum_required(VERSION 3.20)

project(libupd C)


# ---- include external deps ----
include(CTest)
include(TestBigEndian)

add_subdirectory(thirdparty)


# ---- env check ----
test_big_endian(UPD_BIG_ENDIAN)
if (UPD_BIG_ENDIAN)
  message(FATAL_ERROR "big endian is not supported")
endif()

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  set(UPD_C_FLAGS -Wall -Wextra -Werror -pedantic -Wno-missing-field-initializers)
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  set(UPD_C_FLAGS /W2 /std:c11 /Zc:preprocessor-)
else()
  message(ERROR "unknown compiler")
endif()


# ---- core library ----
add_library(libupd INTERFACE)
target_include_directories(libupd SYSTEM BEFORE
  INTERFACE .
)
target_sources(libupd
  INTERFACE
    libupd.h
    libupd/array.h
    libupd/buf.h
    libupd/memory.h
    libupd/msgpack.h
    libupd/path.h
    libupd/pathfind.h
)
target_link_libraries(libupd
  INTERFACE
    hedley
    msgpackc
    utf8.h
)


# ---- test app ----
if(BUILD_TESTING)
  add_executable(libupd-test)
  target_link_libraries(libupd-test
    PRIVATE
      libupd
  )
  target_compile_options(libupd-test
    PRIVATE ${UPD_C_FLAGS}
  )
  target_sources(libupd-test
    PRIVATE
      test.c
  )
  add_test(
    NAME    libupd
    COMMAND $<TARGET_FILE:libupd-test>
  )
endif()
