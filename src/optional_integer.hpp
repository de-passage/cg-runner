#ifndef HEADER_GUARD_DPSG_OPTIONAL_INTEGER_HPP
#define HEADER_GUARD_DPSG_OPTIONAL_INTEGER_HPP

#include "meta/identity_type.hpp"

#include <cassert>
#include <numeric>
#include <optional>
#include <type_traits>

namespace dpsg {

template <class Underlying = int,
          Underlying Tombstone = std::is_signed_v<Underlying>
                                     ? std::numeric_limits<Underlying>::min()
                                     : std::numeric_limits<Underlying>::max()>
class optional_integer {
  Underlying _value = Tombstone;

public:
  using type = Underlying;
  using reference = type &;
  using const_reference = const type &;
  constexpr static inline type tombstone = Tombstone;

  constexpr optional_integer() noexcept = default;
  constexpr optional_integer(type value) noexcept : _value(value) {}
  constexpr optional_integer(std::nullopt_t) noexcept : _value(tombstone) {}
  constexpr optional_integer(const optional_integer &) noexcept = default;
  constexpr optional_integer(optional_integer &&) noexcept = default;
  constexpr optional_integer &
  operator=(const optional_integer &) noexcept = default;
  constexpr optional_integer &operator=(optional_integer &&) noexcept = default;
  constexpr optional_integer &operator=(type value) noexcept {
    _value = value;
    return *this;
  }
  constexpr optional_integer &operator=(std::nullopt_t) noexcept {
    _value = tombstone;
    return *this;
  }
  constexpr bool has_value() const noexcept { return _value != tombstone; }
  constexpr explicit operator bool() const noexcept { return has_value(); }
  constexpr type value() const noexcept { return _value; }
  constexpr type value_or(type def) const noexcept {
    return has_value() ? _value : def;
  }
  constexpr type operator*() const noexcept {
    assert(has_value() && "optional_integer has no value");
    return value();
  }
  constexpr reference operator*() noexcept {
    assert(has_value() && "optional_integer has no value");
    return value();
  }
  constexpr type operator->() const noexcept {
    assert(has_value() && "optional_integer has no value");
    return value();
  }
  constexpr type *operator->() noexcept {
    assert(has_value() && "optional_integer has no value");
    return value();
  }
  constexpr bool operator==(const optional_integer &other) const noexcept {
    return _value == other._value;
  }
  constexpr bool operator!=(const optional_integer &other) const noexcept {
    return _value != other._value;
  }
  constexpr bool operator==(type other) const noexcept {
    return _value == other;
  }
  constexpr bool operator!=(type other) const noexcept {
    return _value != other;
  }
  constexpr bool operator==(std::nullopt_t) const noexcept {
    return !has_value();
  }
  constexpr bool operator!=(std::nullopt_t) const noexcept {
    return has_value();
  }
};
} // namespace dpsg

#endif // HEADER_GUARD_DPSG_OPTIONAL_INTEGER_HPP
