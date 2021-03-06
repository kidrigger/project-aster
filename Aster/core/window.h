// =============================================
//  Aster: window.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>

#include <core/context.h>
#include <ownership.h>

struct Window final {

	Window(const std::string_view& _title, Borrowed<Context>&& _context, vk::Extent2D _extent, b8 _full_screen = false);

	Window(const Window& _other) = delete;
	Window(Window&& _other) noexcept;
	Window& operator=(const Window& _other) = delete;
	Window& operator=(Window&& _other) noexcept;

	~Window();

	bool should_close() const noexcept {
		return glfwWindowShouldClose(window);
	}

	bool poll() const noexcept {
		glfwPollEvents();
		return !glfwWindowShouldClose(window);
	}

	void set_window_size(const vk::Extent2D& _extent) noexcept {
		extent = _extent;
		glfwSetWindowSize(window, extent.width, extent.height);
	}

	void set_window_size(const u32 _width, const u32 _height) noexcept {
		set_window_size({ _width, _height });
	}

	// fields
	Borrowed<Context> parent_context{};

	GLFWwindow* window{ nullptr };
	GLFWmonitor* monitor{ nullptr };
	vk::SurfaceKHR surface;
	vk::Extent2D extent;
	std::string name;
	b8 full_screen{ false };
};
