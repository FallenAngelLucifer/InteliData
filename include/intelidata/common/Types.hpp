#pragma once

#include "Platform.hpp"
#include <concepts>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <cstdint>

namespace intelidata {

using Float64 = double;
using Float32 = float;
using Int64   = std::int64_t;
using UInt64  = std::uint64_t;
using Int32   = std::int32_t;
using UInt32  = std::uint32_t;
using Int16   = std::int16_t;
using UInt16  = std::uint16_t;
using Int8    = std::int8_t;
using UInt8   = std::uint8_t;
using Size    = std::size_t;

template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept Numeric = std::floating_point<T> || std::integral<T>;

template <typename T>
concept StringLike = std::same_as<std::decay_t<T>, std::string> ||
                     std::same_as<std::decay_t<T>, std::string_view> ||
                     std::same_as<std::decay_t<T>, const char*>;

} // namespace intelidata
