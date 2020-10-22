/*=========================================*/
/*  Aster: core/window.cc	               */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include "window.h"
#include <logger.h>
#include <core/context.h>
#include <core/glfw_context.h>

void Window::init(const Context* _context, u32 _width, u32 _height, const stl::string& _title, b8 _full_screen) noexcept {
	extent = vk::Extent2D{ .width = _width, .height = _height };
	name = _title;
	full_screen = _full_screen;
	parent_context = _context;

	monitor = glfwGetPrimaryMonitor();

	i32 x_, y_, w_, h_;
	glfwGetMonitorWorkarea(monitor, &x_, &y_, &w_, &h_);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);

	window = glfwCreateWindow(extent.width, extent.height, name.data(), full_screen ? monitor : nullptr, nullptr);
	ERROR_IF(window == nullptr, "Window creation failed") ELSE_INFO(stl::fmt("Window '%s' created with resolution '%dx%d'", name.data(), extent.width, extent.height));
	if (window == nullptr) {
		auto code = GLFWContext::post_error();
		glfwTerminate();
		CRASH(code);
	}

	if (full_screen == false) {
		glfwSetWindowPos(window, (w_ - extent.width) / 2, (h_ - extent.height) / 2);
	}
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	glfwSetWindowUserPointer(window, this);

	glfwSetWindowSizeCallback(window, window_resize_callback);

	VkSurfaceKHR surface_;
	vk::Result result = cast<vk::Result>(glfwCreateWindowSurface(cast<VkInstance>(_context->instance), window, nullptr, &surface_));
	ERROR_IF(failed(result), "Failed to create Surface with "s + to_string(result)) THEN_CRASH(result) ELSE_INFO("Surface Created");
	surface = vk::SurfaceKHR(surface_);
}

void Window::destroy() noexcept {
	parent_context->instance.destroy(surface);
	INFO("Surface Destroyed");

	if (window != nullptr) {
		glfwDestroyWindow(window);
		window = nullptr;
	}
	monitor = nullptr;

	INFO("Window '" + name + "' Destroyed");
}

void Window::window_resize_callback(GLFWwindow* _window, i32 _width, i32 _height) noexcept {
	Window* window = cast<Window*>(glfwGetWindowUserPointer(_window));

	window->extent = vk::Extent2D{ .width = cast<u32>(_width), .height = cast<u32>(_height) };

	for (ResizeCallbackFn& callback_fn : window->resize_callbacks) {
		callback_fn(window->extent);
	}
}
