// =============================================
//  Aster: device.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>
#include <iterator>
#include <core/context.h>
#include <core/window.h>
#include <core/buffer.h>
#include <core/image.h>

#include <span>
#include <string_view>

class Device;

struct QueueFamilyIndices {
	static constexpr u32 invalid_value = 0xFFFFFFFFu;

	u32 graphics_idx{ invalid_value };
	u32 present_idx{ invalid_value };
	u32 compute_idx{ invalid_value };
	u32 transfer_idx{ invalid_value };

	[[nodiscard]]
	b8 has_graphics() const {
		return graphics_idx != invalid_value;
	}

	[[nodiscard]]
	b8 has_present() const {
		return present_idx != invalid_value;
	}

	[[nodiscard]]
	b8 has_compute() const {
		return compute_idx != invalid_value;
	}

	[[nodiscard]]
	b8 has_transfer() const {
		return transfer_idx != invalid_value;
	}
};

struct Queues {
	vk::Queue graphics;
	vk::Queue present;
	vk::Queue transfer;
	Option<vk::Queue> compute;
};

template <typename T>
struct SubmitTask;

class Device {
public:
	struct PhysicalDeviceInfo {
		vk::PhysicalDevice device;
		vk::PhysicalDeviceProperties properties;
		vk::PhysicalDeviceFeatures features;
		QueueFamilyIndices queue_families;

		PhysicalDeviceInfo(const Borrowed<Window>& _window, const vk::PhysicalDevice _device) : device(_device) {
			properties = device.getProperties();
			features = device.getFeatures();
			queue_families = get_queue_families(_window, device);
		}

	private:
		[[nodiscard]] QueueFamilyIndices get_queue_families(const Borrowed<Window>& _window, vk::PhysicalDevice _device) const;
	};


	Device(const std::string_view& _name, const Borrowed<Context>& _parent_context, PhysicalDeviceInfo _physical_device_info, const vk::Device& _device, const Queues& _queues, const vma::Allocator& _allocator, const vk::CommandPool& _transfer_cmd_pool, const vk::CommandPool& _graphics_cmd_pool)
		: parent_context(_parent_context)
		, physical_device(std::move(_physical_device_info))
		, device(_device)
		, queues(_queues)
		, allocator(_allocator)
		, transfer_cmd_pool(_transfer_cmd_pool)
		, graphics_cmd_pool(_graphics_cmd_pool)
		, name(_name) {}

	Device(const Device& _other) = delete;
	Device(Device&& _other) noexcept;
	Device& operator=(const Device& _other) = delete;
	Device& operator=(Device&& _other) noexcept;

	static Res<Device> create(const std::string_view& _name, Borrowed<Context>&& _context, const PhysicalDeviceInfo& _physical_device_info, const vk::PhysicalDeviceFeatures& _enabled_features);

	~Device();

	template <typename T> requires vk::isVulkanHandleType<T>::value
	void set_object_name(const T& _obj, const std::string_view& _name) const {
		auto result = device.setDebugUtilsObjectNameEXT({
			.objectType = _obj.objectType,
			.objectHandle = get_vk_handle(_obj),
			.pObjectName = _name.data(),
		});
		WARN_IF(failed(result), "Debug Utils name setting failed with "s + to_string(result));
	}

	[[nodiscard]]
	Res<vk::CommandBuffer> alloc_temp_command_buffer(vk::CommandPool _pool) const;

	[[nodiscard]] Res<SubmitTask<Buffer>> upload_data(const Borrowed<Buffer>& _host_buffer, Buffer&& _staging_buffer);
	[[nodiscard]] Res<SubmitTask<Buffer>> upload_data(const Borrowed<Buffer>& _host_buffer, const std::span<u8>& _data);
	Res<> update_data(const Borrowed<Buffer>& _host_buffer, const std::span<u8>& _data) const;

	// fields
	Borrowed<Context> parent_context;
	PhysicalDeviceInfo physical_device;
	vk::Device device;
	Queues queues;
	vma::Allocator allocator;

	vk::CommandPool transfer_cmd_pool;
	vk::CommandPool graphics_cmd_pool;

	std::string name;

private:
	void set_name(const std::string_view& _name);
};

template <typename T = void>
struct SubmitTask {
	vk::Fence fence;
	Borrowed<Device> device{};
	T payload;
	std::vector<vk::CommandBuffer> cmd;
	vk::CommandPool pool;

	[[nodiscard]]
	static Res<SubmitTask<T>> create(const Borrowed<Device>& _device, T&& _payload, vk::Queue _queue, vk::CommandPool _pool, const std::vector<vk::CommandBuffer>& _cmd, const std::vector<vk::Semaphore>& _wait_on = {}, const std::vector<vk::Semaphore>& _signal_to = {}) {

		SubmitTask<T> task;
		if (auto res = task.submit(_device, std::forward<T>(_payload), _queue, _pool, _cmd, _wait_on, _signal_to)) {
			return std::move(task);
		} else {
			return Err::make(std::move(res.error()));
		}
	}

	[[nodiscard]]
	Res<> submit(const Borrowed<Device>& _device, T&& _payload, vk::Queue _queue, vk::CommandPool _pool, std::vector<vk::CommandBuffer> _cmd, std::vector<vk::Semaphore> _wait_on = {}, std::vector<vk::Semaphore> _signal_to = {}) {
		device = _device;
		auto [result, _fence] = device->device.createFence({});
		if (failed(result)) {
			return Err::make(std::fmt("Fence creation failed with %s" CODE_LOC, to_cstr(result)), result);
		}
		fence = _fence;
		payload = std::move(_payload);
		cmd = _cmd;
		pool = _pool;

		result = _queue.submit({
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
		if (failed(result)) {
			return Err::make(std::fmt("Submit failed with %s" CODE_LOC, to_cstr(result)), result);
		}

		return {};
	}

	[[nodiscard]]
	Res<> wait_and_destroy() {
		return this->destroy();
	}

	[[nodiscard]]
	Res<> destroy() {
		const auto result = device->device.waitForFences({ fence }, true, max_value<u64>);
		if (failed(result)) return Err::make(std::fmt("Fence wait failed with %s" CODE_LOC, to_cstr(result)), result);
		device->device.destroyFence(fence);
		device->device.freeCommandBuffers(pool, cast<u32>(cmd.size()), cmd.data());
		return {};
	}
};

template <>
struct SubmitTask<void> {
	vk::Fence fence;
	Borrowed<Device> device;
	std::vector<vk::CommandBuffer> cmd;
	vk::CommandPool pool;

	[[nodiscard]]
	static Res<SubmitTask<>> create(const Borrowed<Device>& _device, vk::Queue _queue, vk::CommandPool _pool, const std::vector<vk::CommandBuffer>& _cmd, const std::vector<vk::Semaphore>& _wait_on = {}, const vk::PipelineStageFlags& _wait_stage = vk::PipelineStageFlagBits::eBottomOfPipe, const std::vector<vk::Semaphore>& _signal_to = {}) {

		SubmitTask<> task;
		if (auto res = task.submit(_device, _queue, _pool, _cmd, _wait_on, _wait_stage, _signal_to)) {
			return std::move(task);
		} else {
			return Err::make(std::move(res.error()));
		}
	}

	[[nodiscard]]
	Res<> wait_and_destroy() {
		return this->destroy();
	}

	[[nodiscard]]
	Res<> destroy() {
		const auto result = device->device.waitForFences({ fence }, true, max_value<u64>);
		if (failed(result)) return Err::make(std::fmt("Fence wait failed with %s", to_cstr(result)), result);
		device->device.destroyFence(fence);
		device->device.freeCommandBuffers(pool, cast<u32>(cmd.size()), cmd.data());
		return {};
	}

private:
	[[nodiscard]]
	Res<> submit(const Borrowed<Device>& _device, vk::Queue _queue, vk::CommandPool _pool, std::vector<vk::CommandBuffer> _cmd, std::vector<vk::Semaphore> _wait_on = {},
	             const vk::PipelineStageFlags& _wait_stage = vk::PipelineStageFlagBits::eBottomOfPipe, std::vector<vk::Semaphore> _signal_to = {}) {
		device = _device;
		auto [result, _fence] = device->device.createFence({});
		if (failed(result)) {
			return Err::make(std::fmt("Fence creation failed with %s" CODE_LOC, to_cstr(result)), result);
		}
		fence = _fence;
		cmd = _cmd;
		pool = _pool;
		result = _queue.submit({
				vk::SubmitInfo{
					.waitSemaphoreCount = cast<u32>(_wait_on.size()),
					.pWaitSemaphores = _wait_on.data(),
					.pWaitDstStageMask = &_wait_stage,
					.commandBufferCount = cast<u32>(_cmd.size()),
					.pCommandBuffers = _cmd.data(),
					.signalSemaphoreCount = cast<u32>(_signal_to.size()),
					.pSignalSemaphores = _signal_to.data(),
				} },
			_fence);
		if (failed(result)) {
			return Err::make(std::fmt("Submit failed with %s" CODE_LOC, to_cstr(result)), result);
		}

		return {};
	}
};

class DeviceSelector {
public:
	using PhysicalDeviceInfo = Device::PhysicalDeviceInfo;
	using DeviceSet = std::vector<PhysicalDeviceInfo>;

	class DeviceSelectorIntermediate {
		std::span<PhysicalDeviceInfo> device_set_;
	public:

		explicit DeviceSelectorIntermediate(const std::span<PhysicalDeviceInfo>& _device_set) : device_set_{ _device_set } {}

		template <typename TLambda>
		DeviceSelectorIntermediate select_on(TLambda&& _predicate) {
			auto part = std::partition(device_set_.begin(), device_set_.end(), _predicate);
			return DeviceSelectorIntermediate{ std::span{ device_set_.begin(), part } };
		}

		template <typename TLambda>
		DeviceSelectorIntermediate sort_by(const TLambda& _sorter) {
			std::ranges::sort(device_set_, [key = _sorter](PhysicalDeviceInfo& _a, PhysicalDeviceInfo& _b) {
				return key(_a) > key(_b);
			});
			return DeviceSelectorIntermediate{ device_set_ };
		}

		[[nodiscard]]
		PhysicalDeviceInfo get(const u32 _idx = 0) const {
			ERROR_IF(_idx >= device_set_.size(), "Out of range");
			return device_set_[_idx];
		}

		[[nodiscard]]
		std::vector<PhysicalDeviceInfo> get_all() const {
			return std::vector<PhysicalDeviceInfo>(device_set_.begin(), device_set_.end());
		}
	};

	DeviceSelector(Borrowed<Context>&& _context, Borrowed<Window>&& _window) {
		auto [result, available_physical_devices] = _context->instance.enumeratePhysicalDevices();
		ERROR_IF(failed(result), "Failed fetching devices with "s + to_string(result)) THEN_CRASH(result) ELSE_IF_ERROR(available_physical_devices.empty(), "No valid devices found") THEN_CRASH(Error::eNoDevices);

		std::ranges::transform(available_physical_devices, std::back_inserter(device_set_),
			[window = _window](const vk::PhysicalDevice _dev) {
				return PhysicalDeviceInfo{ window, _dev };
			});
	}

	template <typename TLambda>
	DeviceSelectorIntermediate select_on(TLambda&& _predicate) {
		auto part = std::partition(device_set_.begin(), device_set_.end(), _predicate);
		return DeviceSelectorIntermediate{ std::span{ device_set_.begin(), part } };
	}

	template <typename TLambda>
	DeviceSelectorIntermediate sort_by(const TLambda& _sorter) {
		std::ranges::sort(device_set_, [key = _sorter](PhysicalDeviceInfo& _a, PhysicalDeviceInfo& _b) {
			return key(_a) < key(_b);
		});
		return DeviceSelectorIntermediate{ device_set_ };
	}

	PhysicalDeviceInfo get(const u32 _idx = 0) {
		ERROR_IF(_idx >= device_set_.size(), "Out of range");
		return device_set_[_idx];
	}
	
	std::vector<PhysicalDeviceInfo> get_all() {
		return device_set_;
	}

private:
	DeviceSet device_set_;
};
