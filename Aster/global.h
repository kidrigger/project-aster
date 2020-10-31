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
#include <EASTL/string.h>

#define VULKAN_HPP_ASSERT(expr) DEBUG_IF(!(expr), "Vulkan assert failed")
#include <vulkan/vulkan.hpp>

#pragma warning(push, 0)
#include <vma/vk_mem_alloc.hpp>
#pragma warning(pop)

#include <optick/optick.h>

namespace stl = eastl;

inline bool failed(vk::Result result) {
	return result != vk::Result::eSuccess;
}

namespace eastl {
	template <typename... Ts>
	inline string fmt(const char* fmt, Ts&&... args) {
		return string(string::CtorSprintf(), fmt, args...);
	}
}

inline stl::string operator+(const std::string& c, const stl::string& other) {
	return c.data() + other;
}

inline stl::string operator+(const stl::string& c, const std::string& other) {
	return c + other.data();
}

inline const char* to_cstring(const vk::Result& val) {
	return to_string(val).c_str();
}

// TODO: Check why inline namespaces aren't working in MSVC 19.27.29110
using namespace stl::literals::string_literals;

template <typename T>
[[nodiscard]] constexpr u64 get_vkhandle(const T& d) noexcept {
	return reinterpret_cast<u64>(cast<T::CType>(d));
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
