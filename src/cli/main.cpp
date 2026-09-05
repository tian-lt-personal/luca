// std
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
// 3rd-party
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
// luca
#include <astdump.hpp>
#include <diag.hpp>
#include <eval.hpp>
#include <mp.hpp>
#include <parser.hpp>

namespace {

// directory containing the running luca executable ("" when unknown)
std::string exe_dir(const char* argv0) {
  std::error_code ec;
  std::filesystem::path exe;
#ifdef _WIN32
  std::wstring buf(MAX_PATH, L'\0');
  DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
  if (n == 0 || n >= buf.size()) return {};  // failure or truncated path
  buf.resize(n);
  exe = std::filesystem::path{std::move(buf)};
#elif defined(__linux__)
  exe = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (ec && argv0) exe = std::filesystem::absolute(argv0, ec);
#else
  if (argv0) exe = std::filesystem::absolute(argv0, ec);
#endif
  if (ec || exe.empty()) return {};
  exe.remove_filename();
  return exe.empty() ? std::string{} : exe.lexically_normal().generic_string();
}

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

  std::ifstream ifs;
  ifs.exceptions(std::ios::failbit | std::ios::badbit);
  std::string source;
  try {
    ifs.open(file);
    source.assign(std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{});
  } catch (const std::ios_base::failure&) {
    std::cerr << "error: cannot open file '" << file << "'\n";
    return 1;
  }

  try {
    auto result = parse(source, file, exe_dir(argc > 0 ? argv[0] : nullptr));
    if (dump_mode) {
      std::cout << dump(result.first).dump(2) << '\n';
    } else {
      auto evaluated = evaluate(result.first, eval_strategy::runtime);
      print_value(std::get<value>(evaluated.result));
      std::cout << '\n';
    }
  } catch (const parse_err& e) {
    // a diagnostic from an imported file carries its own source text and path
    std::cerr << render(e.diag, e.src.empty() ? std::string_view{source} : std::string_view{e.src},
                        e.src.empty() ? file : e.filename)
              << '\n';
    return 1;
  } catch (const eval_err& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  } catch (const std::logic_error& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
}
