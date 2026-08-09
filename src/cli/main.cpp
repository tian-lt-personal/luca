// std
#include <fstream>
#include <iostream>
#include <string>
// 3rd-party
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
// luca
#include <astdump.hpp>
#include <machine.hpp>
#include <mp.hpp>
#include <parser.hpp>

int main(int argc, char** argv) {
  CLI::App app{"LUCA — a minifunctional language"};
  std::string file;
  bool dump_mode = false;

  app.add_option("file", file, "Source file path")->required()->check(CLI::ExistingFile);
  app.add_flag("-d", dump_mode, "Output AST as JSON");
  try {
    app.parse(argc, argv);
  } catch (const CLI::Error& e) {
    return app.exit(e);
  }

  std::ifstream ifs{file};
  if (!ifs) {
    std::cerr << "error: cannot open file '" << file << "'\n";
    return 1;
  }
  std::string source{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};

  try {
    auto result = parse(source);
    if (dump_mode) {
      std::cout << dump(result.first).dump(2) << '\n';
    } else {
      auto v = eval(result.first);
      std::visit(overloaded{
                     [](int val) { std::cout << val << '\n'; },
                     [](bool val) { std::cout << (val ? "true" : "false") << '\n'; },
                     [](std::monostate) { std::cout << "()\n"; },
                     [](auto) {},
                 },
                 v);
    }
  } catch (const parse_err& e) {
    std::cerr << "parse error: " << e.what() << '\n';
    return 1;
  } catch (const std::logic_error& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
}
