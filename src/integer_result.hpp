#ifndef HEADER_GUARD_DPSG_INTEGER_RESULT_HPP
#define HEADER_GUARD_DPSG_INTEGER_RESULT_HPP

#include <type_traits>

namespace dpsg {
template <class T, class ErrorType = T> class integer_result {
public:
  using type = T;

private:
  type _value;

public:
  using error_type = ErrorType;
  using reference = type &;

  static_assert(
      (std::is_enum_v<error_type> ||
       std::is_integral_v<error_type>)&&std::is_integral_v<type>,
      "error_type must be an enum or integral type, and type must be integral");
  static_assert(sizeof(type) >= sizeof(error_type),
                "type must be at least as wide as error_type");

  constexpr static inline type error_bit =
      (type)((type)1 << (sizeof(type) * 8 - 1));

  constexpr integer_result() noexcept = default;
  constexpr explicit integer_result(type value) noexcept : _value(value) {}
  template <class U, std::enable_if_t<std::is_convertible_v<U, error_type> &&
                                          !std::is_convertible_v<U, type>,
                                      int> = 0>
  constexpr explicit integer_result(U error) noexcept
      : _value((type)((type)error | error_bit)) {}

  constexpr bool is_error() const { return (error_bit & _value) != 0; }

  constexpr bool is_value() const { return (error_bit & _value) == 0; }

  constexpr type value() const {
    assert(is_value() && "integer_result is an error");
    return _value;
  }

  constexpr reference value() {
    assert(is_value() && "integer_result is an error");
    return _value;
  }

  constexpr error_type error() const {
    assert(is_error() && "integer_result is not an error");
    return (error_type)(_value ^ error_bit);
  }
};
}; // namespace dpsg

#endif // HEADER_GUARD_DPSG_INTEGER_RESULT_HPP
