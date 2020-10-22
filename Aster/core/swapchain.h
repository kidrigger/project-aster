/*=========================================*/
/*  Aster: core/swapchain.h                */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>
#include <core/context.h>
#include <EASTL/fixed_vector.h>

#include <core/window.h>
#include <core/device.h>

struct SurfaceSupportDetails
{
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> present_modes;

	void init(const vk::SurfaceKHR* _surface, const vk::PhysicalDevice* _device) noexcept {
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
	const Window* parent_window;
	const Device* parent_device;

	vk::SwapchainKHR swapchain;
	SurfaceSupportDetails support;
	vk::Format format;
	vk::ColorSpaceKHR color_space;
	vk::PresentModeKHR present_mode;
	vk::Extent2D extent;

	bool requires_ownership_transfer;
	stl::string name;

	std::vector<vk::Image> images;
	std::vector<vk::ImageView> image_views;
	u32 image_count{ 0 };

	void init(const stl::string& _name, const Window* _window, const Device* _device) noexcept;

	void recreate() noexcept {
		ERROR("Unimplemented") THEN_CRASH(0);
	}

	void destroy() noexcept;

	void set_name(const stl::string& _name) noexcept {
		name = _name;
		parent_device->set_object_name(swapchain, name);
	}
};