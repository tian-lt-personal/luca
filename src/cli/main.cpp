// std
#include <fstream>
#include <iostream>
#include <string>
// 3rd-party
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
// luca
#include <astdump.hpp>
#include <diag.hpp>
#include <machine.hpp>
#include <mp.hpp>
#include <parser.hpp>

namespace {

void print_value(const value& v) {
  std::visit(overloaded{
                 [](int val) { std::cout << val; },
                 [](bool val) { std::cout << (val ? "true" : "false"); },
                 [](std::monostate) { std::cout << "()"; },
                 [](tuple_value* t) {
                   std::cout << '(';
                   for (size_t i = 0; i < t->fields.size(); ++i) {
                     if (i) std::cout << ", ";
                     print_value(t->fields[i]);
                   }
                   std::cout << ')';
                 },
                 [](sum_value* s) {
                   std::cout << s->name;
                   if (!std::holds_alternative<std::monostate>(s->payload)) {
                     std::cout << ' ';
                     print_value(s->payload);
                   }
                 },
                 [](const closure*) {},  // functions are never printed
             },
             v);
}

}  // namespace

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
      auto evaluated = eval(result.first);
      print_value(evaluated.v);
      std::cout << '\n';
    }
  } catch (const parse_err& e) {
    std::cerr << render(e.diag, source, file) << '\n';
    return 1;
  } catch (const std::logic_error& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
}
