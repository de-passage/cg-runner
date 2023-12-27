#ifndef HEADER_GUARD_DPSG_CLI_HPP
#define HEADER_GUARD_DPSG_CLI_HPP

#include  "posix.hpp"

namespace dpsg::cli {
enum class parse_error : int {
  invalid_character = 1,
  empty_string = 2,
};
using integer_parse_result = dpsg::integer_result<int, parse_error>;

inline integer_parse_result parse_unsigned_int(std::string_view str) {
  if (str.size() == 0) {
    return integer_parse_result::from_error(parse_error::empty_string);
  }
  int r = 0;
  for (size_t s = 0; s < str.size(); ++s) {
    if (!isdigit(str[s])) {
      return integer_parse_result::from_error(parse_error::invalid_character);
    }
    r *= 10;
    r += str[s] - '0';
  }
  return integer_parse_result{r};
}

} // namespace cli
#endif // HEADER_GUARD_DPSG_CLI_HPP
