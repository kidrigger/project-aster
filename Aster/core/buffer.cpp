// =============================================
//  Aster: buffer.cpp
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "buffer.h"
#include <core/device.h>

Res<Buffer> Buffer::create(const std::string& _name, const Borrowed<Device>& _device, usize _size, vk::BufferUsageFlags _usage, vma::MemoryUsage _memory_usage) {
	auto [result, buffer] = _device->allocator.createBuffer({
		.size = _size,
		.usage = _usage,
		.sharingMode = vk::SharingMode::eExclusive,
	}, {
		.usage = _memory_usage,
	});

	if (failed(result)) {
		return Err::make(std::fmt("Buffer %s creation failed with %s", _name.c_str(), to_cstr(result)));
	}

	_device->set_object_name(buffer.first, _name);

	return Buffer{
		_device,
		buffer.first,
		buffer.second,
		_usage,
		_memory_usage,
		_size,
		_name,
	};
}

Buffer::~Buffer() {
	if (buffer) {
		parent_device->allocator.destroyBuffer(buffer, allocation);
	}
}
