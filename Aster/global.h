/*=========================================*/
/*  Aster: global.h		                   */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
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

#include <optick/optick.h>

namespace stl = std;

inline bool failed(vk::Result _result) {
	return _result != vk::Result::eSuccess;
}

namespace std {
	string internal_fmt_(const char* _fmt, ...);

	template <typename... Ts>
	string fmt(const char* _fmt, Ts&&... args) {
		return internal_fmt_(_fmt, forward<Ts>(args)...);
	}
}

inline const char* to_cstr(const vk::Result& _val) {
	static stl::string cp_buffer = to_string(_val).c_str();
	return cp_buffer.c_str();
}

template <typename T>
const char* to_cstr(const T& _val) {
	static stl::string buffer = to_string(_val).c_str();
	return buffer.c_str();
}

// TODO: Check why inline namespaces aren't working in MSVC 19.27.29110
using namespace stl::literals::string_literals;
using namespace stl::literals::string_view_literals;

template <typename T>
[[nodiscard]] constexpr u64 get_vk_handle(const T& d) noexcept {
	return reinterpret_cast<u64>(cast<T::CType>(d));
}

template <typename F>
struct stl::hash<vk::Flags<F>> {
	usize operator()(const vk::Flags<F>& _val) {
		return stl::hash<u32>()(cast<u32>(_val));
	}
};

template <typename T>
usize hash_any(const T& _val) {
	return stl::hash<stl::remove_cvref_t<T>>()(_val);
}

inline usize hash_combine(usize _hash0, usize _hash1) {
	return _hash0 ^ (_hash1 + 0x9e3779b9 + (_hash0 << 6) + (_hash0 >> 2));
}

struct Time {
	f64 elapsed;
	f64 delta;

	void init() {
		elapsed = glfwGetTime();
		delta = 1.0 / 60.0;
	}

	void update() {
		const auto new_elapsed = glfwGetTime();
		delta = stl::clamp(new_elapsed - elapsed, 0.0, 0.1);
		elapsed = new_elapsed;
	}
};

inline usize closest_multiple(usize _val, usize _of) {
	return _of * ((_val + _of - 1) / _of);
};

extern Time g_time;
