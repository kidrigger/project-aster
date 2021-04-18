// =============================================
//  Aster: image.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once
#include <global.h>

class Device;

struct Image {
	Borrowed<Device> parent_device;
	vk::Image image;
	vma::Allocation allocation;
	vk::ImageUsageFlags usage;
	vma::MemoryUsage memory_usage = vma::MemoryUsage::eUnknown;
	usize size = 0;
	std::string name;

	vk::ImageType type;
	vk::Format format;
	vk::Extent3D extent;
	u32 layer_count{};
	u32 mip_count{};

	Image() = default;

	Image(const Borrowed<Device>& _parent_device, const vk::Image& _image, const vma::Allocation& _allocation, const vk::ImageUsageFlags& _usage, vma::MemoryUsage _memory_usage, usize _size, const std::string& _name, vk::ImageType _type, vk::Format _format, const vk::Extent3D& _extent, u32 _layer_count, u32 _mip_count);

	Image(const Image& _other) = delete;
	Image(Image&& _other) noexcept;
	Image& operator=(const Image& _other) = delete;
	Image& operator=(Image&& _other) noexcept;

	static Res<Image> create(const std::string_view& _name, const Borrowed<Device>& _device, vk::ImageType _image_type, vk::Format _format, const vk::Extent3D& _extent, vk::ImageUsageFlags _usage, u32 _mip_count = 1, vma::MemoryUsage _memory_usage = vma::MemoryUsage::eGpuOnly, u32 _layer_count = 1);

	~Image();
};
