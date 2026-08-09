cmake_minimum_required(VERSION 3.20)
project(luca-cpp VERSION 0.1.0 LANGUAGES CXX)

add_subdirectory(luca)
add_subdirectory(cli)

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()
