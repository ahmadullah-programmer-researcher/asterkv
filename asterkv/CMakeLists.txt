cmake_minimum_required(VERSION 3.20)
project(AsterKV VERSION 0.1.0 LANGUAGES CXX)

option(ASTERKV_BUILD_TESTS "Build AsterKV tests" ON)
option(ASTERKV_BUILD_BENCHMARKS "Build the benchmark executable" ON)
option(ASTERKV_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(asterkv SHARED src/db.cpp src/c_api.cpp)
target_include_directories(asterkv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(asterkv PRIVATE ASTERKV_BUILDING_LIBRARY)
set_target_properties(asterkv PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(ASTERKV_ENABLE_WARNINGS)
  if(MSVC)
    target_compile_options(asterkv PRIVATE /W4 /permissive-)
  else()
    target_compile_options(asterkv PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  endif()
endif()

add_executable(asterkv_cli tools/asterkv_cli.cpp)
set_target_properties(asterkv_cli PROPERTIES OUTPUT_NAME asterkv)
target_link_libraries(asterkv_cli PRIVATE asterkv)

if(ASTERKV_BUILD_TESTS)
  enable_testing()
  find_package(Threads REQUIRED)
  add_executable(asterkv_tests tests/test_db.cpp)
  target_link_libraries(asterkv_tests PRIVATE asterkv Threads::Threads)
  add_test(NAME asterkv_tests COMMAND asterkv_tests)
endif()

if(ASTERKV_BUILD_BENCHMARKS)
  add_executable(asterkv_bench benchmarks/bench.cpp)
  target_link_libraries(asterkv_bench PRIVATE asterkv)
endif()

install(TARGETS asterkv asterkv_cli)
install(DIRECTORY include/ DESTINATION include)

add_executable(asterkv_cpp_example examples/cpp_example.cpp)
target_link_libraries(asterkv_cpp_example PRIVATE asterkv)
