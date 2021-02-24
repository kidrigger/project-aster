// =============================================
//  Aster: window.cc
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "window.h"
#include <logger.h>
#include <core/context.h>
#include <core/glfw_context.h>

Window::Window(const std::string_view& _title, Borrowed<Context>&& _context, vk::Extent2D _extent, b8 _full_screen)
	: parent_context{ std::move(_context) }
	, extent{ _extent }
	, name{ _title }
	, full_screen{ _full_screen } {

	monitor = glfwGetPrimaryMonitor();

	i32 x_, y_, w_, h_;
	glfwGetMonitorWorkarea(monitor, &x_, &y_, &w_, &h_);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);

	window = glfwCreateWindow(extent.width, extent.height, name.data(), full_screen ? monitor : nullptr, nullptr);
	ERROR_IF(window == nullptr, "Window creation failed") ELSE_INFO(std::fmt("Window '%s' created with resolution '%dx%d'", name.data(), extent.width, extent.height));
	if (window == nullptr) {
		auto code = GlfwContext::post_error();
		glfwTerminate();
		CRASH(code);
	}

	if (full_screen == false) {
		glfwSetWindowPos(window, (w_ - extent.width) / 2, (h_ - extent.height) / 2);
	}
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	VkSurfaceKHR surface_;
	auto result = cast<vk::Result>(glfwCreateWindowSurface(cast<VkInstance>(_context->instance), window, nullptr, &surface_));
	ERROR_IF(failed(result), "Failed to create Surface with "s + to_string(result)) THEN_CRASH(result) ELSE_INFO("Surface Created");
	surface = vk::SurfaceKHR(surface_);
}

Window::Window(Window&& _other) noexcept: parent_context{ std::move(_other.parent_context) }
                                        , window{ std::exchange(_other.window, nullptr) }
                                        , monitor{ _other.monitor }
                                        , surface{ std::exchange(_other.surface, nullptr) }
                                        , extent{ _other.extent }
                                        , name{ std::move(_other.name) }
                                        , full_screen{ _other.full_screen } {}

Window& Window::operator=(Window&& _other) noexcept {
	if (this == &_other) return *this;
	parent_context = std::move(_other.parent_context);
	window = _other.window;
	monitor = _other.monitor;
	surface = std::exchange(_other.surface, nullptr);
	extent = _other.extent;
	name = std::move(_other.name);
	full_screen = _other.full_screen;
	return *this;
}

Window::~Window() {
	if (parent_context && surface) {
		parent_context->instance.destroy(surface);
		INFO("Surface Destroyed");
	}

	if (window != nullptr) {
		glfwDestroyWindow(window);
		window = nullptr;
	}
	monitor = nullptr;

	INFO("Window '" + name + "' Destroyed");
}
