/*=========================================*/
/*  Aster: core/window.h                   */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>

#include <core/context.h>

struct Window final {

	const Context* parent_context{ nullptr };

	GLFWwindow* window{ nullptr };
	GLFWmonitor* monitor{ nullptr };
	vk::SurfaceKHR surface;
	vk::Extent2D extent;
	stl::string name;
	b8 full_screen{ false };

	void init(const Context* context, u32 width, u32 height, const stl::string& title, b8 full_screen) noexcept;

	inline bool should_close() noexcept {
		return glfwWindowShouldClose(window);
	}

	inline bool poll() noexcept {
		glfwPollEvents();
		return !glfwWindowShouldClose(window);
	}

	inline void set_window_size(const vk::Extent2D& extent_) noexcept {
		extent = extent_;
		glfwSetWindowSize(window, extent.width, extent.height);
	}

	inline void set_window_size(u32 width, u32 height) noexcept {
		set_window_size({ width, height });
	}

	void destroy() noexcept;
};
