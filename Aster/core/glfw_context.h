/*=========================================*/
/*  Aster: core/glfw_context.h             */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>

struct GlfwContext {
	static i32 post_error() noexcept {
		static const char* error_ = nullptr;
		const auto code = glfwGetError(&error_);
		ERROR("GLFW "s + error_);
		return code;
	}

	inline static u32 count = 0;

	static void init() noexcept {
		if (count++ > 0) return;
		if (glfwInit() == GLFW_FALSE) {
			CRASH(post_error());
		}
	}

	static void destroy() noexcept {
		if (--count == 0) {
			glfwTerminate();
		}
		count = 0;
	}
};
