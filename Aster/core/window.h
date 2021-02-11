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

	void init(const stl::string& _title, const Context* _context, u32 _width, u32 _height, b8 _full_screen) noexcept;

	bool should_close() noexcept {
		return glfwWindowShouldClose(window);
	}

	bool poll() noexcept {
		glfwPollEvents();
		return !glfwWindowShouldClose(window);
	}

	void set_window_size(const vk::Extent2D& _extent) noexcept {
		extent = _extent;
		glfwSetWindowSize(window, extent.width, extent.height);
	}

	void set_window_size(u32 _width, u32 _height) noexcept {
		set_window_size({ _width, _height });
	}

	void destroy() noexcept;
};
