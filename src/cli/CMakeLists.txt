find_package(CLI11 CONFIG REQUIRED)

add_executable(lucacli
  main.cpp
)
target_link_libraries(lucacli PRIVATE luca CLI11::CLI11)
set_target_properties(lucacli PROPERTIES OUTPUT_NAME luca)

include(GNUInstallDirs)
install(TARGETS lucacli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
