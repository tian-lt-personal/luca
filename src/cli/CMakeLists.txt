find_package(CLI11 CONFIG REQUIRED)

add_executable(lucacli
  main.cpp
)
target_link_libraries(lucacli PRIVATE luca CLI11::CLI11)
set_target_properties(lucacli PROPERTIES OUTPUT_NAME luca)

include(GNUInstallDirs)
install(TARGETS lucacli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# the built-in library ships next to the executable (imports fall back to it)
set(LUCA_STDLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../stdlib")

add_custom_command(TARGET lucacli POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${LUCA_STDLIB_DIR}/std.luca" "${LUCA_STDLIB_DIR}/std-ast.luca"
          "$<TARGET_FILE_DIR:lucacli>"
  COMMENT "Copying the built-in library next to the luca executable"
  VERBATIM)

install(FILES "${LUCA_STDLIB_DIR}/std.luca" "${LUCA_STDLIB_DIR}/std-ast.luca" DESTINATION ${CMAKE_INSTALL_BINDIR})
