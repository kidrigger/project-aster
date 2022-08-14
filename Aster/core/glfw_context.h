// =============================================
//  Aster: glfw_context.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>

struct GlfwContext {
	static i32 post_error() noexcept {
		static const char* error_ = nullptr;
		const auto code = glfwGetError(&error_);
		ERROR("GLFW "s + error_);
		return code;
	}

	inline static u32 count = 0;

	GlfwContext() {
		if (count++ > 0) return;
		if (glfwInit() == GLFW_FALSE) {
			CRASH(post_error());
		}
	}

	~GlfwContext() {
		if (--count == 0) {
			glfwTerminate();
		}
		count = 0;
	}
};
