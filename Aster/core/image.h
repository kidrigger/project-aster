// =============================================
//  Aster: image.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>

class Device;

struct Image {
	Device* parent_device;
	vk::Image image;
	vma::Allocation allocation;
	vk::ImageUsageFlags usage;
	vma::MemoryUsage memory_usage = vma::MemoryUsage::eUnknown;
	usize size = 0;
	name_t name;

	vk::ImageType type;
	vk::Format format;
	vk::Extent3D extent;
	u32 layer_count;
	u32 mip_count;

	static Result<Image, vk::Result> create(const std::string& _name, Device* _device, vk::ImageType _image_type, vk::Format _format, const vk::Extent3D& _extent, vk::ImageUsageFlags _usage, u32 _mip_count = 1, vma::MemoryUsage _memory_usage = vma::MemoryUsage::eGpuOnly, u32 _layer_count = 1);

	void destroy();
};
