// =============================================
//  Aster: device.cc
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#include "device.h"

#include <core/context.h>
#include <core/window.h>

#include <core/buffer.h>

#include <vector>
#include <map>
#include <set>

Device::Device(const std::string_view& _name, Context* _context, const PhysicalDeviceInfo& _physical_device_info, const vk::PhysicalDeviceFeatures& _enabled_features)
	: parent_context{ _context }
	, name{ _name } {
	vk::Result result;

	physical_device = _physical_device_info.device;
	physical_device_features = _physical_device_info.features;
	physical_device_properties = _physical_device_info.properties;
	queue_families = _physical_device_info.queue_family_indices;

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

	tie(result, device) = physical_device.createDevice({
		.queueCreateInfoCount = cast<u32>(queue_create_infos.size()),
		.pQueueCreateInfos = queue_create_infos.data(),
		.enabledLayerCount = _context->enable_validation_layers ? cast<u32>(_context->validation_layers.size()) : 0,
		.ppEnabledLayerNames = _context->enable_validation_layers ? _context->validation_layers.data() : nullptr,
		.enabledExtensionCount = cast<u32>(_context->device_extensions.size()),
		.ppEnabledExtensionNames = _context->device_extensions.data(),
		.pEnabledFeatures = &_enabled_features,
	});
	ERROR_IF(failed(result), "Failed to create a logical device with "s + to_string(result)) THEN_CRASH(result) ELSE_INFO("Logical Device Created!");

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

	tie(result, allocator) = vma::createAllocator({
		.physicalDevice = physical_device,
		.device = device,
		.instance = _context->instance,
	});
	ERROR_IF(failed(result), std::fmt("Memory allocator creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Memory Allocator Created");

	set_name(name);

	INFO(std::fmt("Created Device '%s' Successfully", name.data()));

	tie(result, transfer_cmd_pool) = device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = queue_families.transfer_idx,
	});
	ERROR_IF(failed(result), std::fmt("Transfer command pool creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Transfer Command Pool Created");
	set_object_name(transfer_cmd_pool, "Async transfer command pool");

	tie(result, graphics_cmd_pool) = device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = queue_families.graphics_idx,
	});
	ERROR_IF(failed(result), std::fmt("Graphics command pool creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Graphics Command Pool Created");
	set_object_name(graphics_cmd_pool, "Single use Graphics command pool");
}

Device::Device(Device&& _other) noexcept: parent_context{ _other.parent_context }
                                        , physical_device{ std::exchange(_other.physical_device, nullptr) }
                                        , physical_device_properties{ _other.physical_device_properties }
                                        , physical_device_features{ _other.physical_device_features }
                                        , queue_families{ _other.queue_families }
                                        , device{ std::exchange(_other.device, nullptr) }
                                        , queues{ _other.queues }
                                        , allocator{ std::exchange(_other.allocator, nullptr) }
                                        , transfer_cmd_pool{ std::exchange(_other.transfer_cmd_pool, nullptr) }
                                        , graphics_cmd_pool{ std::exchange(_other.graphics_cmd_pool, nullptr) }
                                        , name{ std::move(_other.name) } {}

Device& Device::operator=(Device&& _other) noexcept {
	if (this == &_other) return *this;
	parent_context = _other.parent_context;
	physical_device = std::exchange(_other.physical_device, nullptr);
	physical_device_properties = _other.physical_device_properties;
	physical_device_features = _other.physical_device_features;
	queue_families = _other.queue_families;
	device = std::exchange(_other.device, nullptr);
	queues = _other.queues;
	allocator = std::exchange(_other.allocator, nullptr);
	transfer_cmd_pool = std::exchange(_other.transfer_cmd_pool, nullptr);
	graphics_cmd_pool = std::exchange(_other.graphics_cmd_pool, nullptr);
	name = std::move(_other.name);
	return *this;
}

Device::~Device() {
	if (!device) return;
	if (graphics_cmd_pool) device.destroyCommandPool(graphics_cmd_pool);
	if (transfer_cmd_pool) device.destroyCommandPool(transfer_cmd_pool);
	if (allocator) allocator.destroy();
	device.destroy();
	INFO("Device '" + name + "' Destroyed");
}

SubmitTask<Buffer> Device::upload_data(Buffer* _host_buffer, const std::span<u8>& _data) {
	ERROR_IF(!(_host_buffer->usage & vk::BufferUsageFlagBits::eTransferDst), std::fmt("Buffer %s is not a transfer dst. Use vk::BufferUsageFlagBits::eTransferDst during creation", _host_buffer->name.c_str()))
	ELSE_IF_WARN(_host_buffer->memory_usage != vma::MemoryUsage::eGpuOnly, std::fmt("Memory %s is not GPU only. Upload not required", _host_buffer->name.c_str()));

	auto staging_buffer = Buffer::create(std::fmt("_stage %s", _host_buffer->name.c_str()), this, _data.size(), vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuOnly)
	                      .expect(std::fmt("Staging buffer creation failed with %s", to_cstr(err)));

	update_data(&staging_buffer, _data);

	vk::CommandBuffer cmd;
	vk::CommandBufferAllocateInfo allocate_info = {
		.commandPool = transfer_cmd_pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};

	auto result = device.allocateCommandBuffers(&allocate_info, &cmd);
	set_object_name(cmd, std::fmt("%s transfer command", _host_buffer->name.c_str()));
	ERROR_IF(failed(result), std::fmt("Transfer command pool allocation failed with %s", to_cstr(result))) THEN_CRASH(result);

	result = cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, });
	ERROR_IF(failed(result), std::fmt("Command buffer begin failed with %s", to_cstr(result)));

	cmd.copyBuffer(staging_buffer.buffer, _host_buffer->buffer, {
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = cast<u32>(_data.size())
		} });
	result = cmd.end();
	ERROR_IF(failed(result), std::fmt("Command buffer end failed with %s", to_cstr(result)));

	SubmitTask<Buffer> handle;
	result = handle.submit(this, staging_buffer, queues.transfer, transfer_cmd_pool, { cmd });
	ERROR_IF(failed(result), std::fmt("Submit failed with %s", to_cstr(result))) THEN_CRASH(result);

	return handle;
}

void Device::update_data(Buffer* _host_buffer, const std::span<u8>& _data) const {

	ERROR_IF(_host_buffer->memory_usage != vma::MemoryUsage::eCpuToGpu &&
		_host_buffer->memory_usage != vma::MemoryUsage::eCpuOnly, "Memory is not on CPU so mapping can't be done. Use upload_data");

	auto [result, mapped_memory] = allocator.mapMemory(_host_buffer->allocation);
	ERROR_IF(failed(result), std::fmt("Memory mapping failed with %s", to_cstr(result))) THEN_CRASH(result);
	memcpy(mapped_memory, _data.data(), _data.size());
	allocator.unmapMemory(_host_buffer->allocation);
}

void Device::set_name(const std::string& _name) {
	VERBOSE(std::fmt("Device %s -> %s", name.data(), _name.data()));
	name = _name;
	set_object_name(physical_device, std::fmt("%s GPU", _name.data()));
	set_object_name(device, std::fmt("%s Device", _name.data()));
}

QueueFamilyIndices DeviceSelector::PhysicalDeviceInfo::get_queue_families(const Window* _window, vk::PhysicalDevice _device) const {
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
