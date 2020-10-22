/*=========================================*/
/*  Aster: core/device.h                   */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>

struct Context;
struct Window;

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

struct CommandPools {
	vk::CommandPool transient_transfer;
	vk::CommandPool persistent_graphics;
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

// fields
	vk::PhysicalDevice physical_device;
	vk::PhysicalDeviceProperties physical_device_properties;
	vk::PhysicalDeviceFeatures physical_device_features;
	QueueFamilyIndices queue_families;
	vk::Device device;
	Queues queues;
	CommandPools command_pools;
	vma::Allocator allocator;

	stl::string name;
	void set_name(const stl::string& name) noexcept;
};
