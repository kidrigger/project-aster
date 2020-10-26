/*=========================================*/
/*  Aster: core/device.cc                  */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include "device.h"

#include <core/context.h>
#include <core/window.h>
#include <EASTL/vector.h>
#include <EASTL/vector_set.h>

void Device::init(const stl::string& _name, const Context* _context, const Window* _window) noexcept {
	name = _name;

	vk::Result result;

	std::vector<vk::PhysicalDevice> available_physical_devices;
	// Physical Device
	{
		tie(result, available_physical_devices) = _context->instance.enumeratePhysicalDevices();
		ERROR_IF(failed(result), "Failed fetching devices with "s + to_string(result)) THEN_CRASH(result) ELSE_IF_ERROR(available_physical_devices.empty(), "No valid devices found") THEN_CRASH(Error::eNoDevices);

		stl::vector_map<i32, vk::PhysicalDevice> device_suitability_scores;
		for (auto& i_device : available_physical_devices) {
			device_suitability_scores[device_score(_context, _window, i_device)] = i_device;
		}
		ERROR_IF(device_suitability_scores.back().first <= 0, "No suitable device found") THEN_CRASH(Error::eNoDevices);
		physical_device = device_suitability_scores.back().second;
		physical_device_properties = physical_device.getProperties();
		queue_families = get_queue_families(_window, physical_device);
		physical_device_features = physical_device.getFeatures();
		INFO("Using " + physical_device_properties.deviceName);
	}

	// Logical Device
	stl::vector_map<u32, u32> unique_queue_families;
	unique_queue_families[queue_families.graphics_idx]++;
	unique_queue_families[queue_families.present_idx]++;
	unique_queue_families[queue_families.transfer_idx]++;
	unique_queue_families[queue_families.compute_idx]++;

	stl::array<f32, 4> queue_priority = { 1.0f, 1.0f, 1.0f, 1.0f };
	stl::fixed_vector<vk::DeviceQueueCreateInfo, 4> queue_create_infos;
	for (auto [index_, count_] : unique_queue_families) {
		queue_create_infos.push_back({
			.queueFamilyIndex = index_,
			.queueCount = count_,
			.pQueuePriorities = queue_priority.data(),
		});
	}

	vk::PhysicalDeviceFeatures enabled_device_features = {
		.depthClamp = true,
		.samplerAnisotropy = true,
		.shaderSampledImageArrayDynamicIndexing = true,
	};

	tie(result, device) = physical_device.createDevice({
		.queueCreateInfoCount = cast<u32>(queue_create_infos.size()),
		.pQueueCreateInfos = queue_create_infos.data(),
		.enabledLayerCount = _context->enable_validation_layers ? cast<u32>(_context->validation_layers.size()) : 0,
		.ppEnabledLayerNames = _context->enable_validation_layers ? _context->validation_layers.data() : nullptr,
		.enabledExtensionCount = cast<u32>(_context->device_extensions.size()),
		.ppEnabledExtensionNames = _context->device_extensions.data(),
		.pEnabledFeatures = &enabled_device_features,
	});
	ERROR_IF(failed(result), "Failed to create a logical device with "s + to_string(result)) THEN_CRASH(result) ELSE_INFO("Logical Device Created!");

	// Setup queues
	{
		u32 cidx = --unique_queue_families[queue_families.compute_idx];
		u32 tidx = --unique_queue_families[queue_families.transfer_idx];
		u32 pidx = --unique_queue_families[queue_families.present_idx];
		u32 gidx = --unique_queue_families[queue_families.graphics_idx];

		queues.graphics = device.getQueue(queue_families.graphics_idx, gidx);
		queues.present = device.getQueue(queue_families.present_idx, pidx);
		queues.transfer = device.getQueue(queue_families.transfer_idx, tidx);
		queues.compute = device.getQueue(queue_families.graphics_idx, cidx);
		INFO(stl::fmt("Graphics Queue Index: (%i, %i)", queue_families.graphics_idx, gidx));
		INFO(stl::fmt("Present Queue Index: (%i, %i)", queue_families.present_idx, pidx));
		INFO(stl::fmt("Transfer Queue Index: (%i, %i)", queue_families.transfer_idx, tidx));
		INFO(stl::fmt("Compute Queue Index: (%i, %i)", queue_families.compute_idx, cidx));
	}

	tie(result, allocator) = vma::createAllocator({
		.physicalDevice = physical_device,
		.device = device,
		.instance = _context->instance,
	});
	ERROR_IF(failed(result), "Memory allocator creation failed") THEN_CRASH(result) ELSE_INFO("Memory Allocator Created");

	set_name(name);

	INFO(stl::fmt("Created Device '%s' Successfully", name.data()));
}

void Device::destroy() noexcept {
	allocator.destroy();
	device.destroy();
	INFO("Device '" + name + "' Destroyed");
}

i32 Device::device_score(const Context* _context, const Window* _window, vk::PhysicalDevice _device) noexcept {
	i32 score = 0;
	auto properties = _device.getProperties();
	if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
		score += 2;
	}
	else if (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
		++score;
	}

	auto qfamily_indices = get_queue_families(_window, _device);
	bool qfamily_complete = qfamily_indices.has_graphics() && qfamily_indices.has_present(); // transfer ALWAYS exists if graphics exists
	if (!qfamily_complete) {
		return -1;
	}

	if (qfamily_indices.has_compute()) {
		++score;
	}

	auto [result, extension_properties] = _device.enumerateDeviceExtensionProperties();
	stl::vector<stl::string> exts(extension_properties.size());
	stl::transform(extension_properties.begin(), extension_properties.end(), exts.begin(), [](vk::ExtensionProperties& ext) { return cast<const char*>(ext.extensionName); });
	stl::vector_set<stl::string> extension_set(exts.begin(), exts.end());

	for (const auto& extension : _context->device_extensions) {
		if (extension_set.find(extension) == nullptr) {
			return -1;
		}
	}

	vk::SurfaceCapabilitiesKHR surface_capabilities;
	tie(result, surface_capabilities) = _device.getSurfaceCapabilitiesKHR(_window->surface);

	std::vector<vk::SurfaceFormatKHR> surface_formats;
	tie(result, surface_formats) = _device.getSurfaceFormatsKHR(_window->surface);

	std::vector<vk::PresentModeKHR> present_modes;
	tie(result, present_modes) = _device.getSurfacePresentModesKHR(_window->surface);

	if (surface_formats.empty() || present_modes.empty()) {
		return -1;
	}

	auto device_features = _device.getFeatures();
	if (device_features.samplerAnisotropy) {
		++score;
	}
	if (device_features.shaderSampledImageArrayDynamicIndexing) {
		++score;
	}
	if (device_features.depthClamp) {
		++score;
	}

	DEBUG(stl::fmt("%s Score: %d", properties.deviceName.data(), score));

	return score;
}

void Device::set_name(const stl::string& _name) noexcept {
	VERBOSE(stl::fmt("Device %s -> %s", name.data(), _name.data()));
	name = _name;
	set_object_name(physical_device, stl::fmt("GPU %s", _name.data()));
	set_object_name(device, stl::fmt("Device %s", _name.data()));
}

QueueFamilyIndices Device::get_queue_families(const Window* _window, vk::PhysicalDevice _device) noexcept {
	QueueFamilyIndices indices;

	auto queue_families_ = _device.getQueueFamilyProperties();

	u32 i = 0;
	for (const auto& queueFamily : queue_families_) {
		u32 this_family_count = 0;
		VERBOSE(stl::fmt("Queue(%i): %s", i, to_string(queueFamily.queueFlags).data()));

		if (queueFamily.queueCount <= 0) {
			++i;
			continue;	// Skip families with no queues
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
