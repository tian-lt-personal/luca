// std
#include <algorithm>
#include <string>
// luca
#include "diag.hpp"

std::string render(const diagnostic& d, std::string_view source, std::string_view filename) {
  // line/column from byte offsets, computed on demand
  size_t line = 1, line_start = 0;
  for (size_t i = 0; i < d.loc.begin && i < source.size(); ++i)
    if (source[i] == '\n') {
      ++line;
      line_start = i + 1;
    }
  size_t col = d.loc.begin - line_start + 1;

  size_t line_end = source.find('\n', line_start);
  if (line_end == std::string_view::npos) line_end = source.size();
  size_t caret_len = std::max<size_t>(1, std::min(d.loc.end, line_end) - d.loc.begin);

  std::string out;
  if (!filename.empty()) out += std::string(filename) + ':';
  out += std::to_string(line) + ':' + std::to_string(col) + ": error: " + d.message + '\n';
  out += "  " + std::string(source.substr(line_start, line_end - line_start)) + '\n';
  out += "  " + std::string(col - 1, ' ') + '^' + std::string(caret_len - 1, '~') + '\n';
  if (!d.hint.empty()) out += "hint: " + d.hint + '\n';
  return out;
}
