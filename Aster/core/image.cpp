// =============================================
//  Aster: image.cpp
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "image.h"
#include <core/device.h>

Image::Image(const Borrowed<Device>& _parent_device, const vk::Image& _image, const vma::Allocation& _allocation, const vk::ImageUsageFlags& _usage, vma::MemoryUsage _memory_usage, usize _size, const std::string& _name, vk::ImageType _type, vk::Format _format, const vk::Extent3D& _extent, u32 _layer_count, u32 _mip_count): parent_device(_parent_device)
                                                                                                                                                                                                                                                                                                                                   , image(_image)
                                                                                                                                                                                                                                                                                                                                   , allocation(_allocation)
                                                                                                                                                                                                                                                                                                                                   , usage(_usage)
                                                                                                                                                                                                                                                                                                                                   , memory_usage(_memory_usage)
                                                                                                                                                                                                                                                                                                                                   , size(_size)
                                                                                                                                                                                                                                                                                                                                   , name(_name)
                                                                                                                                                                                                                                                                                                                                   , type(_type)
                                                                                                                                                                                                                                                                                                                                   , format(_format)
                                                                                                                                                                                                                                                                                                                                   , extent(_extent)
                                                                                                                                                                                                                                                                                                                                   , layer_count(_layer_count)
                                                                                                                                                                                                                                                                                                                                   , mip_count(_mip_count) {}

Image::Image(Image&& _other) noexcept: parent_device{ std::move(_other.parent_device) }
                                     , image{ std::exchange(_other.image, nullptr) }
                                     , allocation{ std::exchange(_other.allocation, nullptr) }
                                     , usage{ _other.usage }
                                     , memory_usage{ _other.memory_usage }
                                     , size{ _other.size }
                                     , name{ std::move(_other.name) }
                                     , type{ _other.type }
                                     , format{ _other.format }
                                     , extent{ _other.extent }
                                     , layer_count{ _other.layer_count }
                                     , mip_count{ _other.mip_count } {}

Image& Image::operator=(Image&& _other) noexcept {
	if (this == &_other) return *this;
	std::swap(parent_device, _other.parent_device);
	std::swap(image, _other.image);
	std::swap(allocation, _other.allocation);
	usage = _other.usage;
	memory_usage = _other.memory_usage;
	size = _other.size;
	name = std::move(_other.name);
	type = _other.type;
	format = _other.format;
	extent = _other.extent;
	layer_count = _other.layer_count;
	mip_count = _other.mip_count;
	return *this;
}

Res<Image> Image::create(const std::string_view& _name, const Borrowed<Device>& _device, vk::ImageType _image_type, vk::Format _format, const vk::Extent3D& _extent, vk::ImageUsageFlags _usage, u32 _mip_count, vma::MemoryUsage _memory_usage, u32 _layer_count) {

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
		return Err::make(std::fmt("Image %s creation failed with %s" CODE_LOC, _name.data(), to_cstr(result)), result);
	}

	_device->set_object_name(image.first, _name);

	return Image{
		_device,
		image.first,
		image.second,
		_usage,
		_memory_usage,
		0,
		std::string{ _name },
		_image_type,
		_format,
		_extent,
		_layer_count,
		_mip_count,
	};
}

Image::~Image() {
	if (image && allocation) {
		parent_device->allocator.destroyImage(image, allocation);
	}
}
