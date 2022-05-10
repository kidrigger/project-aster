// =============================================
//  Aster: swapchain.cc
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "swapchain.h"

Swapchain::Swapchain(const std::string_view& _name, Borrowed<Window>&& _window, Borrowed<Device>&& _device)
	: parent_window{ std::move(_window) }
	, parent_device{ std::move(_device) }
	, support{ &_window->surface, &_device->physical_device.device }
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

	requires_ownership_transfer = (_device->physical_device.queue_families.graphics_idx != _device->physical_device.queue_families.present_idx); // needs transfer if not the same queue

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

	if (auto [res, vk_images] = _device->device.getSwapchainImagesKHR(swapchain); !failed(res)) {
		int i_ = 0;
		image_views.clear();
		image_views.reserve(images.size());
		images.reserve(vk_images.size());
		for (auto& vk_image : vk_images) {
			auto name_ = std::fmt("%s Image %u", name.data(), i_++);
			_device->set_object_name(vk_image, name_);
			images.emplace_back(_device, vk_image, vma::Allocation{}, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst, vma::MemoryUsage::eGpuOnly, extent.width * extent.height * 4, name_, vk::ImageType::e2D, format, vk::Extent3D{ extent.width, extent.height, 1 }, 1, 1);
		}
	} else {
		ERROR(std::fmt("Could not fetch images with %s", to_cstr(res))) THEN_CRASH(res);
	}

	auto i_ = 0;
	image_views.clear();
	image_views.reserve(images.size());
	for (auto& image : images) {
		if (auto res = ImageView::create(borrow(image), vk::ImageViewType::e2D, vk::ImageSubresourceRange{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		})) {
			image_views.push_back(std::move(res.value()));
			VERBOSE(std::fmt("Image view %u created", i_++));
		} else {
			ERROR(std::fmt("Image View Creation failed with %s", res.error().what())) THEN_CRASH(res.error().code());
		}
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
	support = SurfaceSupportDetails{ &parent_window->surface, &parent_device->physical_device.device };

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

	requires_ownership_transfer = (parent_device->physical_device.queue_families.graphics_idx != parent_device->physical_device.queue_families.present_idx); // needs transfer if not the same queue

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

	if (auto [res, vk_images] = parent_device->device.getSwapchainImagesKHR(swapchain); !failed(res)) {
		int i_ = 0;
		images.clear();
		images.reserve(vk_images.size());
		for (auto& vk_image : vk_images) {
			auto name_ = std::fmt("%s Image %u", name.data(), i_++);
			parent_device->set_object_name(vk_image, name_);
			images.emplace_back(parent_device, vk_image, vma::Allocation{}, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst, vma::MemoryUsage::eGpuOnly, extent.width * extent.height * 4, name_, vk::ImageType::e2D, format, vk::Extent3D{ extent.width, extent.height, 1 }, 1, 1);
		}
	} else {
		ERROR(std::fmt("Could not fetch images with %s", to_cstr(res))) THEN_CRASH(res);
	}

	const auto res = parent_device->device.waitIdle();
	ERROR_IF(failed(res), std::fmt("Idling failed with %s", to_cstr(result)));

	auto i_ = 0;
	image_views.clear();
	image_views.reserve(images.size());
	for (auto& image : images) {
		if (auto res = ImageView::create(borrow(image), vk::ImageViewType::e2D, vk::ImageSubresourceRange{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		})) {
			image_views.push_back(std::move(res.value()));
			VERBOSE(std::fmt("Image view %u created", i_++));
		} else {
			ERROR(std::fmt("Image View Creation failed with %s", res.error().what())) THEN_CRASH(res.error().code());
		}
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

	if (swapchain) {
		parent_device->device.destroySwapchainKHR(swapchain);
	}
	INFO(std::fmt("Swapchain '%s' destroyed", name.data()));
}
