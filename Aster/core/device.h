/*=========================================*/
/*  Aster: core/device.h                   */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>
#include <core/context.h>
#include <core/window.h>

#include <EASTL/span.h>

struct QueueFamilyIndices {
	static constexpr u32 INVALID_VALUE = 0xFFFFFFFFu;

	u32 graphics_idx{ INVALID_VALUE };
	u32 present_idx{ INVALID_VALUE };
	u32 compute_idx{ INVALID_VALUE };
	u32 transfer_idx{ INVALID_VALUE };

	inline bool has_graphics() const {
		return graphics_idx != INVALID_VALUE;
	}

	inline bool has_present() const {
		return present_idx != INVALID_VALUE;
	}

	inline bool has_compute() const {
		return compute_idx != INVALID_VALUE;
	}

	inline bool has_transfer() const {
		return transfer_idx != INVALID_VALUE;
	}
};

struct Queues {
	vk::Queue graphics;
	vk::Queue present;
	vk::Queue transfer;
	Option<vk::Queue> compute;
};

struct Buffer {
	vk::Buffer buffer;
	vma::Allocation allocation;
	vk::BufferUsageFlags usage;
	usize size;
	stl::string name;
};

struct Device {

	void init(const stl::string& name, const Context* context, const Window* window) noexcept;
	void destroy() noexcept;

	i32 device_score(const Context* context, const Window* window, vk::PhysicalDevice device) noexcept;
	QueueFamilyIndices get_queue_families(const Window* window, vk::PhysicalDevice device) noexcept;

	template <typename T>
	void set_object_name(const T& _obj, const stl::string& _name) const {
		auto result = device.setDebugUtilsObjectNameEXT({
			.objectType = _obj.objectType,
			.objectHandle = get_vkhandle(_obj),
			.pObjectName = _name.data(),
		});
		WARN_IF(failed(result), "Debug Utils name setting failed with "s + to_string(result));
	}

	vk::ResultValue<Buffer> create_buffer(const stl::string& _name, usize _size, vk::BufferUsageFlags _usage, vma::MemoryUsage _memory_usage) {
		auto[result, buffer] = allocator.createBuffer({
			.size = _size,
			.usage = _usage,
			.sharingMode = vk::SharingMode::eExclusive,
		}, {
			.usage = _memory_usage,
		});

		if (!failed(result)) {
			set_object_name(buffer.first, _name);
		}

		return vk::ResultValue<Buffer>(result, { 
			.buffer = buffer.first, 
			.allocation = buffer.second,
			.usage = _usage,
			.size = _size,
			.name = stl::move(_name)
		});
	}

	struct TransferHandle {
		vk::Fence fence;
		const Device* device;
		Buffer staging_buffer;
		vk::CommandBuffer cmd;

		void init(Device* _device, Buffer& _staging_buffer, vk::CommandBuffer _cmd) {
			device = _device;
			auto [result, _fence] = device->device.createFence({});
			ERROR_IF(failed(result), stl::fmt("Fence creation failed with %s", to_cstring(result)));
			_device->set_object_name(_fence, stl::fmt("%s transfer fence", _staging_buffer.name.data()));
			fence = _fence;
			staging_buffer = _staging_buffer;
			cmd = _cmd;
		}

		void wait_for_transfer() {
			this->destroy();
		}

		void destroy() {
			auto result = device->device.waitForFences({ fence }, true, max_value<u64>);
			ERROR_IF(failed(result), stl::fmt("Fence wait failed with %s", to_cstring(result)));
			device->allocator.destroyBuffer(staging_buffer.buffer, staging_buffer.allocation);
			device->device.destroyFence(fence);
			device->device.freeCommandBuffers(device->transfer_cmd_pool, { cmd });
		}
	};

	[[nodiscard]] TransferHandle upload_data(Buffer* _host_buffer, stl::span<u8> _data) {
		auto [result, staging_buffer] = create_buffer("_stage " + _host_buffer->name, _data.size(), vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuToGpu);
		ERROR_IF(failed(result), stl::fmt("Staging buffer creation failed with %s", to_cstring(result))) THEN_CRASH(result);
		{
			void* mapped_memory;
			tie(result, mapped_memory) = allocator.mapMemory(staging_buffer.allocation);
			ERROR_IF(failed(result), stl::fmt("Memory mapping failed with %s", to_cstring(result))) THEN_CRASH(result);
			memcpy(mapped_memory, _data.data(), _data.size());
			allocator.unmapMemory(staging_buffer.allocation);
		};

		vk::CommandBuffer cmd;
		vk::CommandBufferAllocateInfo allocate_info = {
			.commandPool = transfer_cmd_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		};

		result = device.allocateCommandBuffers(&allocate_info, &cmd);
		set_object_name(cmd, stl::fmt("%s transfer command", _host_buffer->name.data()));
		ERROR_IF(failed(result), stl::fmt("Transfer command pool allocation failed with %s", to_cstring(result))) THEN_CRASH(result);

		vk::BufferCopy buffer_copy = {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = cast<u32>(_data.size())
		};

		result = cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, });
		ERROR_IF(failed(result), stl::fmt("Command buffer begin failed with %s", to_cstring(result)));

		cmd.copyBuffer(staging_buffer.buffer, _host_buffer->buffer, { buffer_copy });
		result = cmd.end();
		ERROR_IF(failed(result), stl::fmt("Command buffer end failed with %s", to_cstring(result)));

		TransferHandle handle;
		handle.init(this, staging_buffer, cmd);

		result = queues.transfer.submit({ {
			.commandBufferCount = 1,
			.pCommandBuffers = &cmd,
		} }, handle.fence);
		ERROR_IF(failed(result), stl::fmt("Submit failed with %s", to_cstring(result))) THEN_CRASH(result);

		return handle;
	}

// fields
	vk::PhysicalDevice physical_device;
	vk::PhysicalDeviceProperties physical_device_properties;
	vk::PhysicalDeviceFeatures physical_device_features;
	QueueFamilyIndices queue_families;
	vk::Device device;
	Queues queues;
	vma::Allocator allocator;

	vk::CommandPool transfer_cmd_pool;

	stl::string name;
	void set_name(const stl::string& name) noexcept;
};
