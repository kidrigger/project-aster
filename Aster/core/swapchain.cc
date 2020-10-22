/*=========================================*/
/*  Aster: core/swapchain.cc               */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include "swapchain.h"

void Swapchain::init(const stl::string& _name, const Window* _window, const Device* _device) noexcept {
	parent_window = _window;
	parent_device = _device;
	name = _name;

	VERBOSE("Creating Swapchain formats");
	support.init(&_window->surface, &_device->physical_device);

	VERBOSE("Selecting Surface formats");

	format = vk::Format::eUndefined;
	for (const auto& i_format : support.formats) {
		if (i_format.format == vk::Format::eB8G8R8A8Srgb && i_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			format = i_format.format;
			color_space = i_format.colorSpace;
			break;
		}
	}
	ERROR_IF(format == vk::Format::eUndefined, "No valid swapchain format found") THEN_CRASH(0) ELSE_VERBOSE("Selected format: "s + to_string(format) + " and colorspace: "s + to_string(color_space));

	present_mode = vk::PresentModeKHR::eFifo;
	for (const auto& i_mode : support.present_modes) {
		if (i_mode == vk::PresentModeKHR::eMailbox) {
			present_mode = i_mode;
			break;
		}
		else if (i_mode == vk::PresentModeKHR::eImmediate) {
			present_mode = i_mode;
		}
	}

	if (support.capabilities.currentExtent.width != max_value<u32>) {
		extent = support.capabilities.currentExtent;
	}
	else {
		extent.width = stl::clamp(_window->extent.width, support.capabilities.minImageExtent.width, support.capabilities.maxImageExtent.width);
		extent.height = stl::clamp(_window->extent.width, support.capabilities.minImageExtent.width, support.capabilities.maxImageExtent.width);
	}

	image_count = support.capabilities.minImageCount + 1;
	if (support.capabilities.maxImageCount > 0) {
		image_count = stl::min(image_count, support.capabilities.maxImageCount);
	}

	requires_ownership_transfer = (_device->queue_families.graphics_idx != _device->queue_families.present_idx);	// needs transfer if not the same queue

	vk::Result result;
	tie(result, swapchain) = _device->device.createSwapchainKHR({
		.surface = _window->surface,
		.minImageCount = image_count,
		.imageFormat = format,
		.imageColorSpace = color_space,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.preTransform = support.capabilities.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = vk::SwapchainKHR(),
		});
	ERROR_IF(failed(result), stl::fmt("Swapchain '%s' creation failed with %s", name.data(), to_cstring(result))) ELSE_INFO(stl::fmt("Swapchain '%s' created!", name.data()));

	_device->set_object_name(swapchain, name);

	tie(result, images) = _device->device.getSwapchainImagesKHR(swapchain);
	ERROR_IF(failed(result), stl::fmt("Could not fetch images with %s", to_cstring(result))) THEN_CRASH(result);

	u32 i_ = 0;
	for (auto& image_ : images) {
		_device->set_object_name(image_, stl::fmt("%s_image_%u", name.data(), i_++));
	}

	i_ = 0;
	for (auto& image : images) {
		tie(result, image_views.emplace_back()) = parent_device->device.createImageView({
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.components = {},	// All Identity
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		});
		ERROR_IF(failed(result), stl::fmt("Image View Creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE(stl::fmt("Image view %u created", i_++));
	}

	i_ = 0;
	for (auto& image_ : images) {
		_device->set_object_name(image_, stl::fmt("%s_view_%u", name.data(), i_++));
	}

	INFO(stl::fmt("Number of swapchain images in %s %d", name.data(), image_count));
}

void Swapchain::destroy() noexcept {
	for (auto& image_view : image_views) {
		parent_device->device.destroyImageView(image_view);
	}
	parent_device->device.destroySwapchainKHR(swapchain);
	INFO(stl::fmt("Swapchain '%s' destroyed", name.data()));
}
