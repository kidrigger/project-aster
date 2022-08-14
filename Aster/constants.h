// =============================================
//  Aster: constants.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once

#include <cstdint>
#include <cstdio>
#include <optional>
#include <tuple>

#include <glm/glm.hpp>

using c8 = char;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using f128 = long double;
using b8 = bool;
using b32 = u32;
using usize = size_t;
using p64 = intptr_t;

constexpr usize strlen_c(const char* s) {
	return *s == '\0' ? 0 : 1 + strlen_c(s + 1);
}

constexpr auto ANSI_BLACK = "\u001b[30m";
constexpr auto ANSI_RED = "\u001b[31m";
constexpr auto ANSI_GREEN = "\u001b[32m";
constexpr auto ANSI_YELLOW = "\u001b[33m";
constexpr auto ANSI_BLUE = "\u001b[34m";
constexpr auto ANSI_MAGENTA = "\u001b[35m";
constexpr auto ANSI_CYAN = "\u001b[36m";
constexpr auto ANSI_WHITE = "\u001b[37m";
constexpr auto ANSI_RESET = "\u001b[0m";

using std::move;
using std::forward;
using std::tie;

template <typename T>
using Option = std::optional<T>;

template <typename type_t, typename from_t>
constexpr auto cast(from_t&& _in) {
	return static_cast<type_t>(forward<from_t>(_in));
}

template <typename type_t, typename from_t>
constexpr auto recast(from_t&& _in) {
	return reinterpret_cast<type_t>(forward<from_t>(_in));
}

constexpr f32 operator ""_deg(f128 _degrees) {
	return glm::radians<f32>(cast<f32>(_degrees));
}

constexpr f32 operator ""_deg(u64 _degrees) {
	return glm::radians<f32>(cast<f32>(_degrees));
}

using glm::ivec2;
using glm::ivec3;
using glm::ivec4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

using glm::mat2;
using glm::mat3;
using glm::mat4;

constexpr const char* PROJECT_NAME = "Aster";

struct Version {
	u32 major;
	u32 minor;
	u32 patch;
};

constexpr Version VERSION = {
	.major = 0,
	.minor = 0,
	.patch = 1,
};

enum class ErrorCode {
	eUnknown = 1000,
	eNoDevices = 1001,
};

template <typename T>
constexpr T MAX_VALUE = std::numeric_limits<T>::max();

template <typename T>
constexpr T MIN_VALUE = std::numeric_limits<T>::min();

template <typename T>
constexpr T LOWEST_VALUE = std::numeric_limits<T>::lowest();

template <typename T>
constexpr T ERR_EPSILON = std::numeric_limits<T>::epsilon();

template <typename T>
constexpr T POSITIVE_INF = std::numeric_limits<T>::infinity();

template <typename T>
constexpr T NEGATIVE_INF = -std::numeric_limits<T>::infinity();

template <typename T>
constexpr T Q_NAN = std::numeric_limits<T>::quiet_NaN();

template <typename T>
constexpr T S_NAN = std::numeric_limits<T>::signalling_NaN();
