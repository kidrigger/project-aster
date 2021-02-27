// =============================================
//  Aster: buffer.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>

struct Device;

struct Buffer {
	Borrowed<Device> parent_device;
	vk::Buffer buffer;
	vma::Allocation allocation;
	vk::BufferUsageFlags usage;
	vma::MemoryUsage memory_usage = vma::MemoryUsage::eUnknown;
	usize size = 0;
	std::string name;

	Buffer() = default;

	Buffer(const Borrowed<Device>& _parent_device, const vk::Buffer& _buffer, const vma::Allocation& _allocation, const vk::BufferUsageFlags& _usage, vma::MemoryUsage _memory_usage, usize _size, const std::string& _name)
		: parent_device{ _parent_device }
		, buffer(_buffer)
		, allocation(_allocation)
		, usage(_usage)
		, memory_usage(_memory_usage)
		, size(_size)
		, name(_name) {}

	Buffer(const Buffer& _other) = delete;

	Buffer(Buffer&& _other) noexcept
		: parent_device{ std::move(_other.parent_device) }
		, buffer{ std::exchange(_other.buffer, nullptr) }
		, allocation{ std::exchange(_other.allocation, nullptr) }
		, usage{ _other.usage }
		, memory_usage{ _other.memory_usage }
		, size{ _other.size }
		, name{ std::move(_other.name) } {}

	Buffer& operator=(const Buffer& _other) = delete;

	Buffer& operator=(Buffer&& _other) noexcept {
		if (this == &_other) return *this;
		std::swap(parent_device, _other.parent_device);
		std::swap(buffer, _other.buffer);
		std::swap(allocation, _other.allocation);
		usage = _other.usage;
		memory_usage = _other.memory_usage;
		size = _other.size;
		name = std::move(_other.name);
		return *this;
	}

	static Res<Buffer> create(const std::string& _name, const Borrowed<Device>& _device, usize _size, vk::BufferUsageFlags _usage, vma::MemoryUsage _memory_usage);

	~Buffer();
};
