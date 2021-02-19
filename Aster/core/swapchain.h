/*=========================================*/
/*  Aster: core/swapchain.h                */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>
#include <core/context.h>
#include <core/device.h>
#include <core/window.h>

#include <vector>

struct SurfaceSupportDetails {
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> present_modes;

	SurfaceSupportDetails(const vk::SurfaceKHR* _surface, const vk::PhysicalDevice* _device) {
		vk::Result result;
		tie(result, capabilities) = _device->getSurfaceCapabilitiesKHR(*_surface);
		ERROR_IF(failed(result), "Fetching surface capabilities failed with "s + to_string(result)) THEN_CRASH(result);

		tie(result, formats) = _device->getSurfaceFormatsKHR(*_surface);
		ERROR_IF(failed(result), "Fetching surface formats failed with "s + to_string(result)) THEN_CRASH(result);

		tie(result, present_modes) = _device->getSurfacePresentModesKHR(*_surface);
		ERROR_IF(failed(result), "Fetching surface formats failed with "s + to_string(result)) THEN_CRASH(result);
	}
};

struct Swapchain {
	Window* parent_window;
	Device* parent_device;

	vk::SwapchainKHR swapchain;
	SurfaceSupportDetails support;
	vk::Format format;
	vk::ColorSpaceKHR color_space;
	vk::PresentModeKHR present_mode;
	vk::Extent2D extent;

	bool requires_ownership_transfer;
	std::string name;

	std::vector<vk::Image> images;
	std::vector<vk::ImageView> image_views;
	u32 image_count{ 0 };

	Swapchain(const std::string_view& _name, Window* _window, Device* _device);

	Swapchain(const Swapchain& _other) = delete;
	Swapchain(Swapchain&& _other) noexcept;
	Swapchain& operator=(const Swapchain& _other) = delete;
	Swapchain& operator=(Swapchain&& _other) noexcept;

	~Swapchain();

	void recreate();

	void set_name(const std::string& _name) {
		name = _name;
		parent_device->set_object_name(swapchain, name);
	}
};
