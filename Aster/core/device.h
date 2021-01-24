/*=========================================*/
/*  Aster: core/device.h                   */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>
#include <core/context.h>
#include <core/window.h>

#include <span>
#include <string_view>

struct Device;

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
	Device* parent_device;
	vk::Buffer buffer;
	vma::Allocation allocation;
	vk::BufferUsageFlags usage;
	vma::MemoryUsage memory_usage = vma::MemoryUsage::eUnknown;
	usize size = 0;
	stl::string name;

	static vk::ResultValue<Buffer> create(Device* _device, const stl::string& _name, usize _size, vk::BufferUsageFlags _usage, vma::MemoryUsage _memory_usage);

	void destroy();
};

struct Image {
	Device* parent_device;
	vk::Image image;
	vma::Allocation allocation;
	vk::ImageUsageFlags usage;
	vma::MemoryUsage memory_usage = vma::MemoryUsage::eUnknown;
	usize size = 0;
	stl::string name;

	vk::ImageType type;
	vk::Format format;
	vk::Extent3D extent;
	u32 layer_count;
	u32 mip_count;

	static vk::ResultValue<Image> create(Device* _device, const stl::string& _name, vk::ImageType _image_type, vk::Format _format, const vk::Extent3D& _extent, vk::ImageUsageFlags _usage, u32 _mip_count = 1, vma::MemoryUsage _memory_usage = vma::MemoryUsage::eGpuOnly, u32 _layer_count = 1);

	void destroy();
};

struct Buffer;
template <typename T>
struct SubmitTask;

struct Device {

	void init(const stl::string& name, Context* context, Window* window) noexcept;
	void destroy() noexcept;

	i32 device_score(const Context* context, const Window* window, vk::PhysicalDevice device) noexcept;
	QueueFamilyIndices get_queue_families(const Window* window, vk::PhysicalDevice device);

	template <typename T>
	void set_object_name(const T& _obj, const stl::string_view& _name) const {
		auto result = device.setDebugUtilsObjectNameEXT({
			.objectType = _obj.objectType,
			.objectHandle = get_vkhandle(_obj),
			.pObjectName = _name.data(),
		});
		WARN_IF(failed(result), "Debug Utils name setting failed with "s + to_string(result));
	}

	vk::ResultValue<vk::CommandBuffer> alloc_temp_command_buffer(vk::CommandPool _pool) {
		vk::CommandBuffer cmd;
		vk::CommandBufferAllocateInfo cmd_buf_alloc_info = {
			.commandPool = _pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		};
		const auto result = device.allocateCommandBuffers(&cmd_buf_alloc_info, &cmd);
		return vk::ResultValue<vk::CommandBuffer>(result, cmd);
	}

	[[nodiscard]] SubmitTask<Buffer> upload_data(Buffer* _host_buffer, const stl::span<u8>& _data);
	void update_data(Buffer* _host_buffer, const stl::span<u8>& _data);

// fields
	Context* parent_context;
	vk::PhysicalDevice physical_device;
	vk::PhysicalDeviceProperties physical_device_properties;
	vk::PhysicalDeviceFeatures physical_device_features;
	QueueFamilyIndices queue_families;
	vk::Device device;
	Queues queues;
	vma::Allocator allocator;

	vk::CommandPool transfer_cmd_pool;
	vk::CommandPool graphics_cmd_pool;

	stl::string name;
	void set_name(const stl::string& _name);
};

template <typename T>
struct SubmitTask {
	vk::Fence fence;
	const Device* device;
	T payload;
	stl::vector<vk::CommandBuffer> cmd;
	vk::CommandPool pool;

	vk::Result submit(Device* _device, T& _payload, vk::Queue _queue, vk::CommandPool _pool, stl::vector<vk::CommandBuffer> _cmd, stl::vector<vk::Semaphore> _wait_on = {}, stl::vector<vk::Semaphore> _signal_to = {}) {
		device = _device;
		auto [result, _fence] = device->device.createFence({});
		ERROR_IF(failed(result), stl::fmt("Fence creation failed with %s", to_cstring(result)));
		fence = _fence;
		payload = _payload;
		cmd = _cmd;
		pool = _pool;

		return _queue.submit({
			{
				.waitSemaphoreCount = cast<u32>(_wait_on.size()),
				.pWaitSemaphores = _wait_on.data(),
				.commandBufferCount = cast<u32>(_cmd.size()),
				.pCommandBuffers = _cmd.data(),
				.signalSemaphoreCount = cast<u32>(_signal_to.size()),
				.pSignalSemaphores = _signal_to.data(),
			}
		},
		_fence);
	}

	[[nodiscard]] vk::Result  wait() {
		return this->destroy();
	}

	[[nodiscard]] vk::Result  destroy() {
		auto result = device->device.waitForFences({ fence }, true, max_value<u64>);
		ERROR_IF(failed(result), stl::fmt("Fence wait failed with %s", to_cstring(result)));
		payload.destroy();
		device->device.destroyFence(fence);
		device->device.freeCommandBuffers(pool, cast<u32>(cmd.size()), cmd.data());
		return result;
	}
};


template <>
struct SubmitTask<void> {
	vk::Fence fence;
	const Device* device;
	stl::vector<vk::CommandBuffer> cmd;
	vk::CommandPool pool;

	[[nodiscard]] vk::Result submit(Device* _device, vk::Queue _queue, vk::CommandPool _pool, stl::vector<vk::CommandBuffer> _cmd, stl::vector<vk::Semaphore> _wait_on = {},
		vk::PipelineStageFlags _wait_stage = vk::PipelineStageFlagBits::eBottomOfPipe, stl::vector<vk::Semaphore> _signal_to = {}) {
		device = _device;
		auto [result, _fence] = device->device.createFence({});
		ERROR_IF(failed(result), stl::fmt("Fence creation failed with %s", to_cstring(result)));
		fence = _fence;
		cmd = _cmd;
		pool = _pool;
		return _queue.submit({ vk::SubmitInfo{
			.waitSemaphoreCount = cast<u32>(_wait_on.size()),
			.pWaitSemaphores = _wait_on.data(),
			.pWaitDstStageMask = &_wait_stage,
			.commandBufferCount = cast<u32>(_cmd.size()),
			.pCommandBuffers = _cmd.data(),
			.signalSemaphoreCount = cast<u32>(_signal_to.size()),
			.pSignalSemaphores = _signal_to.data(),
		} },
		_fence);
	}

	[[nodiscard]] vk::Result wait() {
		return this->destroy();
	}

	[[nodiscard]] vk::Result destroy() {
		auto result = device->device.waitForFences({ fence }, true, max_value<u64>);
		ERROR_IF(failed(result), stl::fmt("Fence wait failed with %s", to_cstring(result)));
		device->device.destroyFence(fence);
		device->device.freeCommandBuffers(pool, cast<u32>(cmd.size()), cmd.data());
		return result;
	}
};
