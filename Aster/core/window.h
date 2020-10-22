/*=========================================*/
/*  Aster: core/window.h                   */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>
#include <EASTL/string.h>
#include <EASTL/fixed_slist.h>

struct Context;

struct Window final {

	const Context* parent_context{ nullptr };

	GLFWwindow* window{ nullptr };
	GLFWmonitor* monitor{ nullptr };
	vk::SurfaceKHR surface;
	vk::Extent2D extent;
	stl::string name;
	b8 full_screen{ false };

	using ResizeCallbackFn = stl::function<void(const vk::Extent2D&)>;
	stl::slist<ResizeCallbackFn> resize_callbacks;
	using CallbackHandle = stl::slist<ResizeCallbackFn>::const_iterator;

	void init(const Context* context, u32 width, u32 height, const stl::string& title, b8 full_screen) noexcept;

	inline bool should_close() noexcept {
		return glfwWindowShouldClose(window);
	}

	inline bool poll() noexcept {
		glfwPollEvents();
		return !glfwWindowShouldClose(window);
	}

	void set_window_size(const vk::Extent2D& extent_) noexcept {
		extent = extent_;
		glfwSetWindowSize(window, extent.width, extent.height);
		for (auto& callback_fn : resize_callbacks) {
			callback_fn(extent);
		}
	}

	void set_window_size(u32 width, u32 height) noexcept {
		set_window_size({ width, height });
	}

	static void window_resize_callback(GLFWwindow* window, i32 width, i32 height) noexcept;

	CallbackHandle add_resize_callback(ResizeCallbackFn&& fn) {
		resize_callbacks.push_front(stl::move(fn));
		return resize_callbacks.cbegin();
	}

	void remove_resize_callback(CallbackHandle& handle) {
		resize_callbacks.erase(handle);
	}

	void destroy() noexcept;
};
