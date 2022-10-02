// =============================================
//  Aster: buffer.cc
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#include <core/buffer.h>
#include <core/device.h>

Result<Buffer, vk::Result> Buffer::create(const std::string& _name, Device* _device, usize _size, vk::BufferUsageFlags _usage, vma::MemoryUsage _memory_usage) {
	auto [result, buffer] = _device->allocator.createBuffer({
		.size = _size,
		.usage = _usage,
		.sharingMode = vk::SharingMode::eExclusive,
	}, {
		.usage = _memory_usage,
	});

	if (failed(result)) {
		return make_error(result);
	}

	_device->set_object_name(buffer.first, _name);

	return Buffer{
		.parent_device = _device,
		.buffer = buffer.first,
		.allocation = buffer.second,
		.usage = _usage,
		.memory_usage = _memory_usage,
		.size = _size,
		.name = name_t::from(_name),
	};
}

void Buffer::destroy() {
	if (parent_device && allocation && buffer) {
		parent_device->allocator.destroyBuffer(buffer, allocation);
		parent_device = nullptr;
		buffer = nullptr;
		allocation = nullptr;
	}
}
