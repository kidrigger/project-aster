/*=========================================*/
/*  Aster: core/glfw_context.h             */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>

struct GLFWContext {
	static inline i32 post_error() noexcept {
		static const char* error_ = nullptr;
		i32 code = glfwGetError(&error_);
		ERROR("GLFW "s + error_);
		return code;
	}

	inline static u32 count = 0;

	void init() noexcept {
		if (count++ > 0) return;
		if (glfwInit() == GLFW_FALSE) {
			CRASH(post_error());
		}
	}

	void destroy() noexcept {
		if (--count == 0) {
			glfwTerminate();
		}
		count = 0;
	}
};