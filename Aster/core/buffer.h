// =============================================
//  Aster: buffer.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once
#include <stdafx.h>

class Device;

struct Buffer {
	Device* parent_device;
	vk::Buffer buffer;
	vma::Allocation allocation;
	vk::BufferUsageFlags usage;
	vma::MemoryUsage memory_usage = vma::MemoryUsage::eUnknown;
	usize size = 0;
	name_t name;

	static Result<Buffer, vk::Result> create(const std::string& _name, Device* _device, usize _size, vk::BufferUsageFlags _usage, vma::MemoryUsage _memory_usage);

	void destroy();
};
