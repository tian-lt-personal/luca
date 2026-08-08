find_program(RE2C_EXECUTABLE NAMES re2c REQUIRED)

set(LEXER_RE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/lexer.re")
set(LEXER_CPP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/lexer.cpp")

add_custom_command(
  OUTPUT "${LEXER_CPP_FILE}"
  COMMAND "${RE2C_EXECUTABLE}" --utf-8 -W -Werror -o "${LEXER_CPP_FILE}" "${LEXER_RE_FILE}"
  DEPENDS "${LEXER_RE_FILE}"
  COMMENT "Generating lexer.cpp from lexer.re using re2c"
  VERBATIM
)

add_library(luca STATIC
  lexer.cpp
  parser.cpp
)
target_compile_features(luca PUBLIC cxx_std_23)
target_include_directories(luca PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
