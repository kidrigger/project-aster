// =============================================
//  Aster: global.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once

#include <config.h>
#include <constants.h>
#include <logger.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <string>

#define VULKAN_HPP_ASSERT(expr) DEBUG_IF(!(expr), "Vulkan assert failed")
#include <vulkan/vulkan.hpp>

#pragma warning(push, 0)
#include <vma/vk_mem_alloc.hpp>
#pragma warning(pop)

[[nodiscard]]
inline bool failed(const vk::Result _result) {
	return _result != vk::Result::eSuccess;
}

namespace std {
	namespace impl {
		string format(const char* _fmt, ...);
	}

	template <typename... Ts>
	[[nodiscard]]
	string fmt(const char* _fmt, Ts&&... _args) {
		return impl::format(_fmt, forward<Ts>(_args)...);
	}
}

[[nodiscard]]
inline const char* to_cstr(const vk::Result& _val) {
	static std::string cp_buffer;
	cp_buffer = to_string(_val);
	return cp_buffer.c_str();
}

template <typename T>
[[nodiscard]]
const char* to_cstr(const T& _val) {
	static std::string buffer;
	buffer = to_string(_val);
	return buffer.c_str();
}

// TODO: Check why inline namespaces aren't working in MSVC 19.27.29110
using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

template <typename T>
[[nodiscard]]
constexpr u64 get_vk_handle(const T& d) noexcept {
	return reinterpret_cast<u64>(cast<T::CType>(d));
}

template <typename F>
struct std::hash<vk::Flags<F>> {
	[[nodiscard]]
	usize operator()(const vk::Flags<F>& _val) {
		return std::hash<u32>()(cast<u32>(_val));
	}
};

template <typename T>
[[nodiscard]]
usize hash_any(const T& _val) {
	return std::hash<std::remove_cvref_t<T>>()(_val);
}

[[nodiscard]]
inline usize hash_combine(usize _hash0, usize _hash1) {
	constexpr usize salt_value = 0x9e3779b9;
	return _hash0 ^ (_hash1 + salt_value + (_hash0 << 6) + (_hash0 >> 2));
}

struct Time {
	static constexpr f64 max_delta = 0.1;

	f64 elapsed;
	f64 delta;

	void init() {
		elapsed = glfwGetTime();
		delta = 1.0 / 60.0;
	}

	void update() {
		const auto new_elapsed = glfwGetTime();
		delta = std::clamp(new_elapsed - elapsed, 0.0, max_delta);
		elapsed = new_elapsed;
	}
};

[[nodiscard]]
constexpr usize closest_multiple(usize _val, usize _of) {
	return _of * ((_val + _of - 1) / _of);
}

constexpr usize MAX_NAME_LENGTH = 31;
struct name_t {
	constexpr static usize SIZE = MAX_NAME_LENGTH + 1;
	constexpr static char END_CHAR = '\0';

	c8 data[SIZE];

	void write(const std::string_view _str) {
		strcpy_s(data, SIZE, _str.data());
	}

	static name_t from(const std::string_view& _str) {
		name_t n;
		n.write(_str);
		return n;
	}

	name_t& operator=(const std::string_view& _str) {
		write(_str);
		return *this;
	}

	[[nodiscard]] const c8* c_str() const { return data; }
};

static_assert(std::is_trivially_copyable_v<name_t>);

extern Time g_time;
