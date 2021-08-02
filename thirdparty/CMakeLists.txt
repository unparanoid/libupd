set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING     OFF)


# ---- hedley (https://github.com/nemequ/hedley) ----
add_library(hedley INTERFACE)
target_include_directories(hedley SYSTEM BEFORE
  INTERFACE hedley
)
target_sources(hedley
  INTERFACE hedley/hedley.h
)


# ---- libyaml (https://github.com/yaml/libyaml) ----
add_subdirectory(libyaml)
add_library(libyaml ALIAS yaml)


# ---- msgpack-c (https://github.com/msgpack/msgpack-c) ----
set(MSGPACK_ENABLE_STATIC  ON)
set(MSGPACK_BUILD_EXAMPLES OFF)
add_subdirectory(msgpack-c)


# ---- utf8.h (https://github.com/sheredom/utf8.h) ----
add_library(utf8.h INTERFACE)
target_include_directories(utf8.h SYSTEM BEFORE
  INTERFACE utf8.h
)
target_sources(utf8.h
  INTERFACE utf8.h/utf8.h
)
