/*=========================================*/
/*  Aster: constants.h	                   */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <config.h>

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

constexpr auto ANSI_Black = "\u001b[30m";
constexpr auto ANSI_Red = "\u001b[31m";
constexpr auto ANSI_Green = "\u001b[32m";
constexpr auto ANSI_Yellow = "\u001b[33m";
constexpr auto ANSI_Blue = "\u001b[34m";
constexpr auto ANSI_Magenta = "\u001b[35m";
constexpr auto ANSI_Cyan = "\u001b[36m";
constexpr auto ANSI_White = "\u001b[37m";
constexpr auto ANSI_Reset = "\u001b[0m";

using std::move;
using std::forward;
using std::tie;

template <typename T>
using Option = std::optional<T>;

template <typename type_t, typename from_t>
inline constexpr auto cast(from_t&& _in) { return static_cast<type_t>(forward<from_t>(_in)); }

constexpr f32 operator ""_deg(f128 degrees) {
	return glm::radians<f32>(cast<f32>(degrees));
}

constexpr f32 operator ""_deg(u64 degrees) {
	return glm::radians<f32>(cast<f32>(degrees));
}

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

enum class Error {
	eUnknown = 1000,
	eNoDevices = 1001,
};

template <typename T>
constexpr T max_value = std::numeric_limits<T>::max();

template <typename T>
constexpr T min_value = std::numeric_limits<T>::min();

template <typename T>
constexpr T lowest_value = std::numeric_limits<T>::lowest();

template <typename T>
constexpr T err_epsilon = std::numeric_limits<T>::epsilon();

template <typename T>
constexpr T positive_inf = std::numeric_limits<T>::infinity();

template <typename T>
constexpr T negative_inf = -std::numeric_limits<T>::infinity();

template <typename T>
constexpr T qnan = std::numeric_limits<T>::quiet_NaN();

template <typename T>
constexpr T snan = std::numeric_limits<T>::signalling_NaN();

