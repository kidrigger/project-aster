// =============================================
//  Aster: image.cc
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#include "image.h"
#include <core/device.h>

Result<Image, vk::Result> Image::create(const std::string& _name, Device* _device, vk::ImageType _image_type, vk::Format _format, const vk::Extent3D& _extent, vk::ImageUsageFlags _usage, u32 _mip_count, vma::MemoryUsage _memory_usage, u32 _layer_count) {

	auto [result, image] = _device->allocator.createImage({
		.imageType = _image_type,
		.format = _format,
		.extent = _extent,
		.mipLevels = _mip_count,
		.arrayLayers = _layer_count,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = _usage,
		.sharingMode = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined,
	}, {
		.usage = _memory_usage,
	});

	if (failed(result)) {
		return make_error(result);
	}

	_device->set_object_name(image.first, _name);

	return Image{
		.parent_device = _device,
		.image = image.first,
		.allocation = image.second,
		.usage = _usage,
		.memory_usage = _memory_usage,
		.name = name_t::from(_name),
		.type = _image_type,
		.format = _format,
		.extent = _extent,
		.layer_count = _layer_count,
		.mip_count = _mip_count,
	};
}

void Image::destroy() {
	if (parent_device && allocation && image) {
		parent_device->allocator.destroyImage(image, allocation);
		parent_device = nullptr;
		allocation = nullptr;
		image = nullptr;
	}
}
