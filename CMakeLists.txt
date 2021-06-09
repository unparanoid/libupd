cmake_minimum_required(VERSION 3.20)

project(libupd C)


# ---- include external deps ----
include(CTest)

add_subdirectory(thirdparty)


# ---- core library ----
add_library(libupd INTERFACE)
target_include_directories(libupd SYSTEM
  INTERFACE .
)
target_sources(libupd
  INTERFACE
    libupd.h
    libupd/array.h
    libupd/buf.h
    libupd/memory.h
    libupd/path.h
    libupd/pathfind.h
)
target_link_libraries(libupd
  INTERFACE
    hedley
    utf8.h
)


# ---- test app ----
if(BUILD_TESTING)
  add_executable(libupd-test)
  target_link_libraries(libupd-test
    PRIVATE
      libupd
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
