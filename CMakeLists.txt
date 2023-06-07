project(fynth-ng)
cmake_minimum_required(VERSION 3.2)
add_definitions("-std=gnu++17 -ggdb3")

add_executable(
  fynth-ng
  effects.cpp
  error.cpp
  filter.cpp
  main.cpp
  pipewire.cpp
  sample.cpp
  sound.cpp
)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE RelWithDebInfo)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(fynth-ng Threads::Threads)

include(FindPkgConfig)

pkg_check_modules(SNDFILE REQUIRED sndfile)
target_link_libraries(fynth-ng ${SNDFILE_LIBRARIES})
target_include_directories(fynth-ng PUBLIC ${SNDFILE_INCLUDE_DIRS})
target_compile_options(fynth-ng PUBLIC ${SNDFILE_CFLAGS_OTHER})

# pipewire
find_package(PipeWire)
pkg_check_modules(PIPEWIRE libpipewire-0.3)
target_link_libraries(fynth-ng ${PIPEWIRE_LIBRARIES})
target_include_directories(fynth-ng PUBLIC ${PIPEWIRE_INCLUDE_DIRS})
target_compile_options(fynth-ng PUBLIC ${PIPEWIRE_CFLAGS_OTHER})
