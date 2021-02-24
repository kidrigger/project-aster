// =============================================
//  Aster: swapchain.cc
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "swapchain.h"

Swapchain::Swapchain(const std::string_view& _name, Borrowed<Window>&& _window, Borrowed<Device>&& _device)
	: parent_window{ std::move(_window) }
	, parent_device{ std::move(_device) }
	, support{ &_window->surface, &_device->physical_device }
	, name{ _name } {

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
		if (i_mode == vk::PresentModeKHR::eImmediate) {
			present_mode = i_mode;
		}
	}

	if (support.capabilities.currentExtent.width != max_value<u32>) {
		extent = support.capabilities.currentExtent;
	} else {
		extent.width = std::clamp(_window->extent.width, support.capabilities.minImageExtent.width, support.capabilities.maxImageExtent.width);
		extent.height = std::clamp(_window->extent.width, support.capabilities.minImageExtent.width, support.capabilities.maxImageExtent.width);
	}

	image_count = support.capabilities.minImageCount + 1;
	if (support.capabilities.maxImageCount > 0) {
		image_count = std::min(image_count, support.capabilities.maxImageCount);
	}

	requires_ownership_transfer = (_device->queue_families.graphics_idx != _device->queue_families.present_idx); // needs transfer if not the same queue

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
		.clipped = true,
		.oldSwapchain = vk::SwapchainKHR(),
	});
	ERROR_IF(failed(result), std::fmt("Swapchain '%s' creation failed with %s", name.data(), to_cstr(result))) ELSE_INFO(std::fmt("Swapchain '%s' created!", name.data()));

	_device->set_object_name(swapchain, name);

	tie(result, images) = _device->device.getSwapchainImagesKHR(swapchain);
	ERROR_IF(failed(result), std::fmt("Could not fetch images with %s", to_cstr(result))) THEN_CRASH(result);

	u32 i_ = 0;
	for (auto& image_ : images) {
		_device->set_object_name(image_, std::fmt("%s Image %u", name.data(), i_++));
	}

	i_ = 0;
	for (auto& image : images) {
		tie(result, image_views.emplace_back()) = parent_device->device.createImageView({
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.components = {}, // All Identity
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		});
		ERROR_IF(failed(result), std::fmt("Image View Creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE(std::fmt("Image view %u created", i_++));
	}

	i_ = 0;
	for (auto& image_ : images) {
		_device->set_object_name(image_, std::fmt("%s View %u", name.data(), i_++));
	}

	INFO(std::fmt("Number of swapchain images in %s %d", name.data(), image_count));
}

Swapchain::Swapchain(Swapchain&& _other) noexcept: parent_window{ std::move(_other.parent_window) }
                                                 , parent_device{ std::move(_other.parent_device) }
                                                 , swapchain{ std::exchange(_other.swapchain, nullptr) }
                                                 , support{ std::move(_other.support) }
                                                 , format{ _other.format }
                                                 , color_space{ _other.color_space }
                                                 , present_mode{ _other.present_mode }
                                                 , extent{ _other.extent }
                                                 , requires_ownership_transfer{ _other.requires_ownership_transfer }
                                                 , name{ std::move(_other.name) }
                                                 , images{ std::move(_other.images) }
                                                 , image_views{ std::move(_other.image_views) }
                                                 , image_count{ _other.image_count } {}

void Swapchain::recreate() {

	VERBOSE("Recreating Swapchain formats");
	support = SurfaceSupportDetails{ &parent_window->surface, &parent_device->physical_device };

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
		if (i_mode == vk::PresentModeKHR::eImmediate) {
			present_mode = i_mode;
		}
	}

	if (support.capabilities.currentExtent.width != max_value<u32>) {
		extent = support.capabilities.currentExtent;
	} else {
		extent.width = std::clamp(parent_window->extent.width, support.capabilities.minImageExtent.width, support.capabilities.maxImageExtent.width);
		extent.height = std::clamp(parent_window->extent.width, support.capabilities.minImageExtent.width, support.capabilities.maxImageExtent.width);
	}

	image_count = support.capabilities.minImageCount + 1;
	if (support.capabilities.maxImageCount > 0) {
		image_count = std::min(image_count, support.capabilities.maxImageCount);
	}

	requires_ownership_transfer = (parent_device->queue_families.graphics_idx != parent_device->queue_families.present_idx); // needs transfer if not the same queue

	vk::Result result;
	tie(result, swapchain) = parent_device->device.createSwapchainKHR({
		.surface = parent_window->surface,
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
		.clipped = true,
		.oldSwapchain = swapchain
	});
	ERROR_IF(failed(result), std::fmt("Swapchain '%s' creation failed with %s", name.data(), to_cstr(result))) ELSE_INFO(std::fmt("Swapchain '%s' recreated!", name.data()));

	parent_device->set_object_name(swapchain, name);

	tie(result, images) = parent_device->device.getSwapchainImagesKHR(swapchain);
	ERROR_IF(failed(result), std::fmt("Could not fetch images with %s", to_cstr(result))) THEN_CRASH(result);

	u32 i_ = 0;
	for (auto& image_ : images) {
		parent_device->set_object_name(image_, std::fmt("%s Image %u", name.data(), i_++));
	}

	// TODO: See if this needs to be / can be async
	result = parent_device->device.waitIdle();
	ERROR_IF(failed(result), std::fmt("Device idling on %s failed with %s", parent_device->name.data(), to_cstr(result))) THEN_CRASH(result);
	for (auto& image_view : image_views) {
		parent_device->device.destroyImageView(image_view);
	}
	image_views.clear();

	i_ = 0;
	for (auto& image : images) {
		tie(result, image_views.emplace_back()) = parent_device->device.createImageView({
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.components = {}, // All Identity
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		});
		ERROR_IF(failed(result), std::fmt("Image View Creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE(std::fmt("Image view %u created", i_++));
	}

	i_ = 0;
	for (auto& image_ : images) {
		parent_device->set_object_name(image_, std::fmt("%s View %u", name.data(), i_++));
	}

	INFO(std::fmt("Number of swapchain images in %s %d", name.data(), image_count));
}

Swapchain& Swapchain::operator=(Swapchain&& _other) noexcept {
	if (this == &_other) return *this;
	parent_window = std::move(_other.parent_window);
	parent_device = std::move(_other.parent_device);
	swapchain = std::exchange(_other.swapchain, nullptr);
	support = std::move(_other.support);
	format = _other.format;
	color_space = _other.color_space;
	present_mode = _other.present_mode;
	extent = _other.extent;
	requires_ownership_transfer = _other.requires_ownership_transfer;
	name = std::move(_other.name);
	images = std::move(_other.images);
	image_views = std::move(_other.image_views);
	image_count = _other.image_count;
	return *this;
}

Swapchain::~Swapchain() {
	for (auto& image_view : image_views) {
		parent_device->device.destroyImageView(image_view);
	}

	if (swapchain) {
		parent_device->device.destroySwapchainKHR(swapchain);
	}
	INFO(std::fmt("Swapchain '%s' destroyed", name.data()));
}
