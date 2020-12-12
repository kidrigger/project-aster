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

inline bool failed(vk::Result result) {
	return result != vk::Result::eSuccess;
}

namespace std {
	string internal_fmt_(const char* fmt, ...);

	template <typename... Ts>
	string fmt(const char* fmt, Ts&&... args) {
		return internal_fmt_(fmt, forward<Ts>(args)...);
	}
}

inline const char* to_cstring(const vk::Result& val) {
	static stl::string cp_buffer = to_string(val).c_str();
	return cp_buffer.c_str();
}

template <typename T>
inline const char* to_cstring(const T& val) {
	static stl::string cstr_buffer = to_string(val).c_str();
	return cstr_buffer.c_str();
}

// TODO: Check why inline namespaces aren't working in MSVC 19.27.29110
using namespace stl::literals::string_literals;
using namespace stl::literals::string_view_literals;

template <typename T>
[[nodiscard]] constexpr u64 get_vkhandle(const T& d) noexcept {
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
		f64 new_elapsed = glfwGetTime();
		delta = stl::clamp(new_elapsed - elapsed, 0.0, 0.1);
		elapsed = new_elapsed;
	}
};

extern Time g_time;
