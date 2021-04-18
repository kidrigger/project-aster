// =============================================
//  Aster: device.cc
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "device.h"

#include <core/context.h>
#include <core/window.h>

#include <vector>
#include <map>
#include <set>

Device::Device(Device&& _other) noexcept: parent_context{ std::move(_other.parent_context) }
                                        , physical_device{ _other.physical_device }
                                        , device{ std::exchange(_other.device, nullptr) }
                                        , queues{ _other.queues }
                                        , allocator{ std::exchange(_other.allocator, nullptr) }
                                        , transfer_cmd_pool{ std::exchange(_other.transfer_cmd_pool, nullptr) }
                                        , graphics_cmd_pool{ std::exchange(_other.graphics_cmd_pool, nullptr) }
                                        , name{ std::move(_other.name) } {}

Device& Device::operator=(Device&& _other) noexcept {
	if (this == &_other) return *this;
	parent_context = std::move(_other.parent_context);
	physical_device = _other.physical_device;
	device = std::exchange(_other.device, nullptr);
	queues = _other.queues;
	allocator = std::exchange(_other.allocator, nullptr);
	transfer_cmd_pool = std::exchange(_other.transfer_cmd_pool, nullptr);
	graphics_cmd_pool = std::exchange(_other.graphics_cmd_pool, nullptr);
	name = std::move(_other.name);
	return *this;
}

Res<Device> Device::create(const std::string_view& _name, Borrowed<Context>&& _context, const PhysicalDeviceInfo& _physical_device_info, const vk::PhysicalDeviceFeatures& _enabled_features) {
	const auto& physical_device = _physical_device_info.device;
	const auto& queue_families = _physical_device_info.queue_families;

	// Logical Device
	std::map<u32, u16> unique_queue_families;
	unique_queue_families[queue_families.graphics_idx]++;
	unique_queue_families[queue_families.present_idx]++;
	unique_queue_families[queue_families.transfer_idx]++;
	unique_queue_families[queue_families.compute_idx]++;

	std::array<f32, 4> queue_priority = { 1.0f, 1.0f, 1.0f, 1.0f };
	std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
	for (auto& [index_, count_] : unique_queue_families) {
		queue_create_infos.push_back({
			.queueFamilyIndex = index_,
			.queueCount = count_,
			.pQueuePriorities = queue_priority.data(),
		});
	}

	vk::Result result;
	vk::Device device;
	tie(result, device) = physical_device.createDevice({
		.queueCreateInfoCount = cast<u32>(queue_create_infos.size()),
		.pQueueCreateInfos = queue_create_infos.data(),
		.enabledLayerCount = _context->enable_validation_layers ? cast<u32>(_context->validation_layers.size()) : 0,
		.ppEnabledLayerNames = _context->enable_validation_layers ? _context->validation_layers.data() : nullptr,
		.enabledExtensionCount = cast<u32>(_context->device_extensions.size()),
		.ppEnabledExtensionNames = _context->device_extensions.data(),
		.pEnabledFeatures = &_enabled_features,
	});
	if (failed(result)) {
		return Err::make(std::fmt("Failed to create a logical device with %s" CODE_LOC, to_cstr(result)), result);
	}

	INFO("Logical Device Created!");

	vma::Allocator allocator;
	std::tie(result, allocator) = vma::createAllocator({
		.physicalDevice = physical_device,
		.device = device,
		.instance = _context->instance,
	});
	if (failed(result)) {
		device.destroy();
		return Err::make(std::fmt("Memory allocator creation failed with %s" CODE_LOC, to_cstr(result)), result);
	}
	VERBOSE("Memory Allocator Created");

	INFO(std::fmt("Created Device '%s' Successfully", _name.data()));

	Queues queues;
	// Setup queues
	{
		u32 compute_idx = --unique_queue_families[queue_families.compute_idx];
		u32 transfer_idx = --unique_queue_families[queue_families.transfer_idx];
		u32 present_idx = --unique_queue_families[queue_families.present_idx];
		u32 graphics_idx = --unique_queue_families[queue_families.graphics_idx];

		queues.graphics = device.getQueue(queue_families.graphics_idx, graphics_idx);
		queues.present = device.getQueue(queue_families.present_idx, present_idx);
		queues.transfer = device.getQueue(queue_families.transfer_idx, transfer_idx);
		queues.compute = device.getQueue(queue_families.graphics_idx, compute_idx);
		INFO(std::fmt("Graphics Queue Index: (%i, %i)", queue_families.graphics_idx, graphics_idx));
		INFO(std::fmt("Present Queue Index: (%i, %i)", queue_families.present_idx, present_idx));
		INFO(std::fmt("Transfer Queue Index: (%i, %i)", queue_families.transfer_idx, transfer_idx));
		INFO(std::fmt("Compute Queue Index: (%i, %i)", queue_families.compute_idx, compute_idx));
	}

	vk::CommandPool transfer_cmd_pool;
	tie(result, transfer_cmd_pool) = device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = queue_families.transfer_idx,
	});
	if (failed(result)) {
		allocator.destroy();
		device.destroy();
		return Err::make(std::fmt("Transfer command pool creation failed with %s" CODE_LOC, to_cstr(result)), result);
	}
	VERBOSE("Transfer Command Pool Created");

	vk::CommandPool graphics_cmd_pool;
	tie(result, graphics_cmd_pool) = device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = queue_families.graphics_idx,
	});
	if (failed(result)) {
		device.destroyCommandPool(transfer_cmd_pool);
		allocator.destroy();
		device.destroy();
		return Err::make(std::fmt("Graphics command pool creation failed with %s" CODE_LOC, to_cstr(result)), result);
	}
	VERBOSE("Graphics Command Pool Created");

	Device final_device{
		_name,
		_context,
		_physical_device_info,
		device,
		queues,
		allocator,
		transfer_cmd_pool,
		graphics_cmd_pool
	};

	final_device.set_name(_name);
	final_device.set_object_name(transfer_cmd_pool, "Async transfer command pool");
	final_device.set_object_name(graphics_cmd_pool, "Single use Graphics command pool");

	return std::move(final_device);
}

Device::~Device() {
	if (!device) return;
	if (graphics_cmd_pool) device.destroyCommandPool(graphics_cmd_pool);
	if (transfer_cmd_pool) device.destroyCommandPool(transfer_cmd_pool);
	if (allocator) allocator.destroy();
	device.destroy();
	INFO("Device '" + name + "' Destroyed");
}

Res<vk::CommandBuffer> Device::alloc_temp_command_buffer(vk::CommandPool _pool) const {
	vk::CommandBuffer cmd;
	vk::CommandBufferAllocateInfo cmd_buf_alloc_info = {
		.commandPool = _pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};
	if (const auto result = device.allocateCommandBuffers(&cmd_buf_alloc_info, &cmd); failed(result)) {
		return Err::make(std::fmt("Temp Command buffer allocation failed with %s" CODE_LOC, to_cstr(result)), result);
	}
	return cmd;
}

Res<SubmitTask<Buffer>> Device::upload_data(const Borrowed<Buffer>& _host_buffer, Buffer&& _staging_buffer) {
	ERROR_IF(!(_host_buffer->usage & vk::BufferUsageFlagBits::eTransferDst), std::fmt("Buffer %s is not a transfer dst. Use vk::BufferUsageFlagBits::eTransferDst during creation", _host_buffer->name.data()))
	ELSE_IF_ERROR(!(_staging_buffer.usage & vk::BufferUsageFlagBits::eTransferSrc), std::fmt("Buffer %s is not a transfer src. Use vk::BufferUsageFlagBits::eTransferSrc during creation", _staging_buffer.name.data()))
	ELSE_IF_WARN(_host_buffer->memory_usage != vma::MemoryUsage::eGpuOnly, std::fmt("Memory %s is not GPU only. Upload not required", _host_buffer->name.data()))
	ELSE_IF_WARN(_host_buffer->memory_usage != vma::MemoryUsage::eCpuOnly, std::fmt("Memory %s is not CPU only. Staging should ideally be a CPU only buffer", _staging_buffer.name.data()));

	vk::CommandBuffer cmd;
	vk::CommandBufferAllocateInfo allocate_info = {
		.commandPool = transfer_cmd_pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};

	auto result = device.allocateCommandBuffers(&allocate_info, &cmd);
	if (failed(result)) {
		return Err::make(std::fmt("Transfer command pool allocation failed with %s" CODE_LOC, to_cstr(result)), result);
	}
	set_object_name(cmd, std::fmt("%s transfer command", _host_buffer->name.data()));

	result = cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, });
	if (failed(result)) {
		return Err::make(std::fmt("Command buffer begin failed with %s" CODE_LOC, to_cstr(result)), result);
	}

	cmd.copyBuffer(_staging_buffer.buffer, _host_buffer->buffer, {
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = cast<u32>(_staging_buffer.size)
		} });
	result = cmd.end();
	if (failed(result)) {
		return Err::make(std::fmt("Command buffer end failed with %s" CODE_LOC, to_cstr(result)), result);
	}

	return SubmitTask<Buffer>::create(borrow(this), std::move(_staging_buffer), queues.transfer, transfer_cmd_pool, { cmd });
}

Res<SubmitTask<Buffer>> Device::upload_data(const Borrowed<Buffer>& _host_buffer, const std::span<u8>& _data) {
	ERROR_IF(!(_host_buffer->usage & vk::BufferUsageFlagBits::eTransferDst), std::fmt("Buffer %s is not a transfer dst. Use vk::BufferUsageFlagBits::eTransferDst during creation", _host_buffer->name.data()))
	ELSE_IF_WARN(_host_buffer->memory_usage != vma::MemoryUsage::eGpuOnly, std::fmt("Memory %s is not GPU only. Upload not required", _host_buffer->name.data()));

	if (auto res = Buffer::create("_stage " + _host_buffer->name, borrow(this), _data.size(), vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuOnly)) {
		return Err::make("Staging buffer creation failed", std::move(res.error()));
	} else {
		if (auto result = update_data(borrow(res.value()), _data)) {
			return upload_data(_host_buffer, std::move(res.value()));
		} else {
			return Err::make(std::move(result.error()));
		}
	}
}

Res<> Device::update_data(const Borrowed<Buffer>& _host_buffer, const std::span<u8>& _data) const {

	if (_host_buffer->memory_usage != vma::MemoryUsage::eCpuToGpu &&
		_host_buffer->memory_usage != vma::MemoryUsage::eCpuOnly) {
		return Err::make("Memory is not on CPU so mapping can't be done. Use upload_data" CODE_LOC);
	}

	auto [result, mapped_memory] = allocator.mapMemory(_host_buffer->allocation);
	if (failed(result)) {
		return Err::make(std::fmt("Memory mapping failed with %s" CODE_LOC, to_cstr(result)), result);
	}
	memcpy(mapped_memory, _data.data(), _data.size());
	allocator.unmapMemory(_host_buffer->allocation);

	return {};
}

void Device::set_name(const std::string_view& _name) {
	VERBOSE(std::fmt("Device %s -> %s", name.data(), _name.data()));
	name = _name;
	set_object_name(physical_device.device, std::fmt("%s GPU", _name.data()));
	set_object_name(device, std::fmt("%s Device", _name.data()));
}

QueueFamilyIndices DeviceSelector::PhysicalDeviceInfo::get_queue_families(const Borrowed<Window>& _window, const vk::PhysicalDevice _device) const {
	QueueFamilyIndices indices;

	auto queue_families_ = _device.getQueueFamilyProperties();

	u32 i = 0;
	for (const auto& queueFamily : queue_families_) {
		u32 this_family_count = 0;
		VERBOSE(std::fmt("Queue(%i): %s", i, to_string(queueFamily.queueFlags).data()));

		if (queueFamily.queueCount <= 0) {
			++i;
			continue; // Skip families with no queues
		}

		if (!indices.has_graphics() && (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)) {
			if (queueFamily.queueCount > this_family_count) {
				indices.graphics_idx = i;
				++this_family_count;
			} else {
				continue;
			}
		}

		if (!indices.has_compute() && (queueFamily.queueFlags & vk::QueueFlagBits::eCompute)) {
			if (queueFamily.queueCount > this_family_count) {
				indices.compute_idx = i;
				++this_family_count;
			} else {
				continue;
			}
		}

		if (!indices.has_transfer() && (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer)) {
			if (queueFamily.queueCount > this_family_count) {
				indices.transfer_idx = i;
				++this_family_count;
			} else {
				continue;
			}
		}

		auto [result, is_present_supported] = _device.getSurfaceSupportKHR(i, _window->surface);
		if (!indices.has_present() && !failed(result) && is_present_supported) {
			if (queueFamily.queueCount > this_family_count) {
				indices.present_idx = i;
				++this_family_count;
			} else {
				continue;
			}
		}

		++i;
	}

	return indices;
}
