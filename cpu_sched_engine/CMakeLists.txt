cmake_minimum_required(VERSION 3.10)
project(schedsim VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Engine as a static library so the CLI and the tests share the exact same code.
file(GLOB ENGINE_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)
list(REMOVE_ITEM ENGINE_SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)
add_library(schedsim_core STATIC ${ENGINE_SOURCES})
target_include_directories(schedsim_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(MSVC)
  target_compile_options(schedsim_core PRIVATE /W4)
else()
  target_compile_options(schedsim_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(schedsim src/main.cpp)
target_link_libraries(schedsim PRIVATE schedsim_core)

enable_testing()
add_executable(test_engine tests/test_engine.cpp)
target_link_libraries(test_engine PRIVATE schedsim_core)
add_test(NAME engine_tests COMMAND test_engine)
