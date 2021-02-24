// =============================================
//  Aster: device.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>
#include <iterator>
#include <core/context.h>
#include <core/window.h>

#include <span>
#include <string_view>

struct Device;

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

struct Buffer {
	Borrowed<Device> parent_device;
	vk::Buffer buffer;
	vma::Allocation allocation;
	vk::BufferUsageFlags usage;
	vma::MemoryUsage memory_usage = vma::MemoryUsage::eUnknown;
	usize size = 0;
	std::string name;

	/*Buffer(const Borrowed<Device>& _parent_device, const vk::Buffer& _buffer, const vma::Allocation& _allocation, const vk::BufferUsageFlags& _usage, vma::MemoryUsage _memory_usage, usize _size, const std::string& _name)
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
		, buffer{ _other.buffer }
		, allocation{ _other.allocation }
		, usage{ _other.usage }
		, memory_usage{ _other.memory_usage }
		, size{ _other.size }
		, name{ std::move(_other.name) } {}

	Buffer& operator=(const Buffer& _other) = delete;

	Buffer& operator=(Buffer&& _other) noexcept {
		if (this == &_other) return *this;
		parent_device = std::move(_other.parent_device);
		buffer = _other.buffer;
		allocation = _other.allocation;
		usage = _other.usage;
		memory_usage = _other.memory_usage;
		size = _other.size;
		name = std::move(_other.name);
		return *this;
	}*/

	static vk::ResultValue<Buffer> create(const std::string& _name, const Borrowed<Device>& _device, usize _size, vk::BufferUsageFlags _usage, vma::MemoryUsage _memory_usage);

	void destroy();
};

struct Image {
	Borrowed<Device> parent_device;
	vk::Image image;
	vma::Allocation allocation;
	vk::ImageUsageFlags usage;
	vma::MemoryUsage memory_usage = vma::MemoryUsage::eUnknown;
	usize size = 0;
	std::string name;

	vk::ImageType type;
	vk::Format format;
	vk::Extent3D extent;
	u32 layer_count;
	u32 mip_count;

	static vk::ResultValue<Image> create(const std::string_view& _name, const Borrowed<Device>& _device, vk::ImageType _image_type, vk::Format _format, const vk::Extent3D& _extent, vk::ImageUsageFlags _usage, u32 _mip_count = 1, vma::MemoryUsage _memory_usage = vma::MemoryUsage::eGpuOnly, u32 _layer_count = 1);

	void destroy();
};

struct ImageView {
	Borrowed<Image> parent_image;
	vk::ImageView image_view;
	vk::Format format;
	vk::ImageViewType type;
	vk::ImageSubresourceRange subresource_range;
	std::string name;

	static vk::ResultValue<ImageView> create(const Borrowed<Image>& _image, vk::ImageViewType _image_type, const vk::ImageSubresourceRange& _subresource_range);

	void destroy() const;
};

template <typename T>
struct SubmitTask;

struct Device {
	struct PhysicalDeviceInfo {
		vk::PhysicalDevice device;
		vk::PhysicalDeviceProperties properties;
		vk::PhysicalDeviceFeatures features;
		QueueFamilyIndices queue_family_indices;

		PhysicalDeviceInfo(const Borrowed<Window>& _window, const vk::PhysicalDevice _device) : device(_device) {
			properties = device.getProperties();
			features = device.getFeatures();
			queue_family_indices = get_queue_families(_window, device);
		}

	private:
		QueueFamilyIndices get_queue_families(const Borrowed<Window>& _window, vk::PhysicalDevice _device) const;
	};

	Device(const std::string_view& _name, Borrowed<Context>&& _context, const PhysicalDeviceInfo& _physical_device_info, const vk::PhysicalDeviceFeatures& _enabled_features);

	Device(const Device& _other) = delete;
	Device(Device&& _other) noexcept;
	Device& operator=(const Device& _other) = delete;
	Device& operator=(Device&& _other) noexcept;

	~Device();

	template <typename T>
	void set_object_name(const T& _obj, const std::string_view& _name) const {
		auto result = device.setDebugUtilsObjectNameEXT({
			.objectType = _obj.objectType,
			.objectHandle = get_vk_handle(_obj),
			.pObjectName = _name.data(),
		});
		WARN_IF(failed(result), "Debug Utils name setting failed with "s + to_string(result));
	}

	vk::ResultValue<vk::CommandBuffer> alloc_temp_command_buffer(vk::CommandPool _pool) const {
		vk::CommandBuffer cmd;
		vk::CommandBufferAllocateInfo cmd_buf_alloc_info = {
			.commandPool = _pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		};
		const auto result = device.allocateCommandBuffers(&cmd_buf_alloc_info, &cmd);
		return vk::ResultValue<vk::CommandBuffer>(result, cmd);
	}

	[[nodiscard]] SubmitTask<Buffer> upload_data(Buffer* _host_buffer, const std::span<u8>& _data);
	void update_data(Buffer* _host_buffer, const std::span<u8>& _data) const;

	// fields
	Borrowed<Context> parent_context;
	vk::PhysicalDevice physical_device;
	vk::PhysicalDeviceProperties physical_device_properties;
	vk::PhysicalDeviceFeatures physical_device_features;
	QueueFamilyIndices queue_families;
	vk::Device device;
	Queues queues;
	vma::Allocator allocator;

	vk::CommandPool transfer_cmd_pool;
	vk::CommandPool graphics_cmd_pool;

	std::string name;

private:
	void set_name(const std::string& _name);
};

template <typename T>
struct SubmitTask {
	vk::Fence fence;
	Borrowed<Device> device{};
	Owned<T> payload;
	std::vector<vk::CommandBuffer> cmd;
	vk::CommandPool pool;

	SubmitTask() = default;

	vk::Result submit(const Borrowed<Device>& _device, Owned<T>&& _payload, vk::Queue _queue, vk::CommandPool _pool, std::vector<vk::CommandBuffer> _cmd, std::vector<vk::Semaphore> _wait_on = {}, std::vector<vk::Semaphore> _signal_to = {}) {
		device = _device;
		auto [result, _fence] = device->device.createFence({});
		ERROR_IF(failed(result), std::fmt("Fence creation failed with %s", to_cstr(result)));
		fence = _fence;
		payload = std::move(_payload);
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

	[[nodiscard]] vk::Result wait() {
		return this->destroy();
	}

	[[nodiscard]] vk::Result destroy() {
		const auto result = device->device.waitForFences({ fence }, true, max_value<u64>);
		ERROR_IF(failed(result), std::fmt("Fence wait failed with %s", to_cstr(result)));
		device->device.destroyFence(fence);
		device->device.freeCommandBuffers(pool, cast<u32>(cmd.size()), cmd.data());
		return result;
	}
};

template <>
struct SubmitTask<void> {
	vk::Fence fence;
	Borrowed<Device> device;
	std::vector<vk::CommandBuffer> cmd;
	vk::CommandPool pool;

	[[nodiscard]] vk::Result submit(const Borrowed<Device>& _device, vk::Queue _queue, vk::CommandPool _pool, std::vector<vk::CommandBuffer> _cmd, std::vector<vk::Semaphore> _wait_on = {},
	                                const vk::PipelineStageFlags& _wait_stage = vk::PipelineStageFlagBits::eBottomOfPipe, std::vector<vk::Semaphore> _signal_to = {}) {
		device = _device;
		auto [result, _fence] = device->device.createFence({});
		ERROR_IF(failed(result), std::fmt("Fence creation failed with %s", to_cstr(result)));
		fence = _fence;
		cmd = _cmd;
		pool = _pool;
		return _queue.submit({
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
	}

	[[nodiscard]] vk::Result wait_and_destroy() {
		return this->destroy();
	}

	[[nodiscard]] vk::Result destroy() {
		const auto result = device->device.waitForFences({ fence }, true, max_value<u64>);
		ERROR_IF(failed(result), std::fmt("Fence wait failed with %s", to_cstr(result)));
		device->device.destroyFence(fence);
		device->device.freeCommandBuffers(pool, cast<u32>(cmd.size()), cmd.data());
		return result;
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

		PhysicalDeviceInfo get(const u32 _idx = 0) const {
			ERROR_IF(_idx >= device_set_.size(), "Out of range");
			return device_set_[_idx];
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

private:
	DeviceSet device_set_;
};
