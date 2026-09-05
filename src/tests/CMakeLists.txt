find_package(GTest CONFIG REQUIRED)

add_executable(luca_tests
  lexer_tests.cpp
  parser_tests.cpp
  astdump_tests.cpp
  eval_tests.cpp
  diag_tests.cpp
  module_tests.cpp
)

target_link_libraries(luca_tests PRIVATE
  luca
  GTest::gtest
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(luca_tests)
