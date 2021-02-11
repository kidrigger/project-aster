/*=========================================*/
/*  Aster: core/device.cc                  */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include "device.h"

#include <core/context.h>
#include <core/window.h>

#include <vector>
#include <map>
#include <set>

void Device::init(const stl::string& _name, Context* _context, Window* _window) noexcept {
	name = _name;
	parent_context = _context;

	vk::Result result;

	std::vector<vk::PhysicalDevice> available_physical_devices;
	// Physical Device
	{
		tie(result, available_physical_devices) = _context->instance.enumeratePhysicalDevices();
		ERROR_IF(failed(result), "Failed fetching devices with "s + to_string(result)) THEN_CRASH(result) ELSE_IF_ERROR(available_physical_devices.empty(), "No valid devices found") THEN_CRASH(Error::eNoDevices);

		stl::map<i32, vk::PhysicalDevice> device_suitability_scores;
		for (auto& i_device : available_physical_devices) {
			device_suitability_scores[device_score(_context, _window, i_device)] = i_device;
		}
		ERROR_IF(device_suitability_scores.rbegin()->first <= 0, "No suitable device found") THEN_CRASH(Error::eNoDevices);
		physical_device = device_suitability_scores.rbegin()->second;
		physical_device_properties = physical_device.getProperties();
		queue_families = get_queue_families(_window, physical_device);
		physical_device_features = physical_device.getFeatures();
		INFO(stl::fmt("Using %s", physical_device_properties.deviceName.data()));
	}

	// Logical Device
	stl::map<u32, u16> unique_queue_families;
	unique_queue_families[queue_families.graphics_idx]++;
	unique_queue_families[queue_families.present_idx]++;
	unique_queue_families[queue_families.transfer_idx]++;
	unique_queue_families[queue_families.compute_idx]++;

	stl::array<f32, 4> queue_priority = { 1.0f, 1.0f, 1.0f, 1.0f };
	stl::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
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
	ERROR_IF(failed(result), stl::fmt("Memory allocator creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Memory Allocator Created");

	set_name(name);

	INFO(stl::fmt("Created Device '%s' Successfully", name.data()));

	tie(result, transfer_cmd_pool) = device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = queue_families.transfer_idx,
	});
	ERROR_IF(failed(result), stl::fmt("Transfer command pool creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Transfer Command Pool Created");
	set_object_name(transfer_cmd_pool, "Async transfer command pool");

	tie(result, graphics_cmd_pool) = device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = queue_families.graphics_idx,
	});
	ERROR_IF(failed(result), stl::fmt("Graphics command pool creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Graphics Command Pool Created");
	set_object_name(graphics_cmd_pool, "Single use Graphics command pool");
}

void Device::destroy() noexcept {
	device.destroyCommandPool(graphics_cmd_pool);
	device.destroyCommandPool(transfer_cmd_pool);
	allocator.destroy();
	device.destroy();
	INFO("Device '" + name + "' Destroyed");
}

i32 Device::device_score(const Context* _context, const Window* _window, vk::PhysicalDevice _device) noexcept {
	i32 score = 0;
	auto properties = _device.getProperties();
	if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
		score += 2;
	} else if (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
		++score;
	}

	auto queue_family_indices = get_queue_families(_window, _device);
	auto queue_family_complete = queue_family_indices.has_graphics() && queue_family_indices.has_present(); // transfer ALWAYS exists if graphics exists
	if (!queue_family_complete) {
		return -1;
	}

	if (queue_family_indices.has_compute()) {
		++score;
	}

	auto [result, extension_properties] = _device.enumerateDeviceExtensionProperties();
	stl::vector<stl::string> extensions(extension_properties.size());
	stl::ranges::transform(extension_properties, extensions.begin(), [](vk::ExtensionProperties& _ext) {
		return cast<const char*>(_ext.extensionName);
	});
	stl::set<stl::string> extension_set(extensions.begin(), extensions.end());

	for (const auto& extension : _context->device_extensions) {
		if (extension_set.find(extension) == extension_set.end()) {
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

SubmitTask<Buffer> Device::upload_data(Buffer* _host_buffer, const stl::span<u8>& _data) {
	ERROR_IF(!(_host_buffer->usage & vk::BufferUsageFlagBits::eTransferDst), stl::fmt("Buffer %s is not a transfer dst. Use vk::BufferUsageFlagBits::eTransferDst during creation", _host_buffer->name.data()))
	ELSE_IF_WARN(_host_buffer->memory_usage != vma::MemoryUsage::eGpuOnly, stl::fmt("Memory %s is not GPU only. Upload not required", _host_buffer->name.data()));

	auto [result, staging_buffer] = Buffer::create("_stage " + _host_buffer->name, this, _data.size(), vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuOnly);
	ERROR_IF(failed(result), stl::fmt("Staging buffer creation failed with %s", to_cstr(result))) THEN_CRASH(result);

	update_data(&staging_buffer, _data);

	vk::CommandBuffer cmd;
	vk::CommandBufferAllocateInfo allocate_info = {
		.commandPool = transfer_cmd_pool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};

	result = device.allocateCommandBuffers(&allocate_info, &cmd);
	set_object_name(cmd, stl::fmt("%s transfer command", _host_buffer->name.data()));
	ERROR_IF(failed(result), stl::fmt("Transfer command pool allocation failed with %s", to_cstr(result))) THEN_CRASH(result);

	result = cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, });
	ERROR_IF(failed(result), stl::fmt("Command buffer begin failed with %s", to_cstr(result)));

	cmd.copyBuffer(staging_buffer.buffer, _host_buffer->buffer, {
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = cast<u32>(_data.size())
		} });
	result = cmd.end();
	ERROR_IF(failed(result), stl::fmt("Command buffer end failed with %s", to_cstr(result)));

	SubmitTask<Buffer> handle;
	result = handle.submit(this, staging_buffer, queues.transfer, transfer_cmd_pool, { cmd });
	ERROR_IF(failed(result), stl::fmt("Submit failed with %s", to_cstr(result))) THEN_CRASH(result);

	return handle;
}

void Device::update_data(Buffer* _host_buffer, const stl::span<u8>& _data) {

	ERROR_IF(_host_buffer->memory_usage != vma::MemoryUsage::eCpuToGpu &&
		_host_buffer->memory_usage != vma::MemoryUsage::eCpuOnly, "Memory is not on CPU so mapping can't be done. Use upload_data");

	auto [result, mapped_memory] = allocator.mapMemory(_host_buffer->allocation);
	ERROR_IF(failed(result), stl::fmt("Memory mapping failed with %s", to_cstr(result))) THEN_CRASH(result);
	memcpy(mapped_memory, _data.data(), _data.size());
	allocator.unmapMemory(_host_buffer->allocation);
}

void Device::set_name(const stl::string& _name) {
	VERBOSE(stl::fmt("Device %s -> %s", name.data(), _name.data()));
	name = _name;
	set_object_name(physical_device, stl::fmt("%s GPU", _name.data()));
	set_object_name(device, stl::fmt("%s Device", _name.data()));
}

QueueFamilyIndices Device::get_queue_families(const Window* _window, vk::PhysicalDevice _device) {
	QueueFamilyIndices indices;

	auto queue_families_ = _device.getQueueFamilyProperties();

	u32 i = 0;
	for (const auto& queueFamily : queue_families_) {
		u32 this_family_count = 0;
		VERBOSE(stl::fmt("Queue(%i): %s", i, to_string(queueFamily.queueFlags).data()));

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

vk::ResultValue<Buffer> Buffer::create(const stl::string& _name, Device* _device, usize _size, vk::BufferUsageFlags _usage, vma::MemoryUsage _memory_usage) {
	auto [result, buffer] = _device->allocator.createBuffer({
		.size = _size,
		.usage = _usage,
		.sharingMode = vk::SharingMode::eExclusive,
	}, {
		.usage = _memory_usage,
	});

	if (!failed(result)) {
		_device->set_object_name(buffer.first, _name);
	}

	return vk::ResultValue<Buffer>(result, {
		.parent_device = _device,
		.buffer = buffer.first,
		.allocation = buffer.second,
		.usage = _usage,
		.memory_usage = _memory_usage,
		.size = _size,
		.name = _name,
	});
}

void Buffer::destroy() {
	parent_device->allocator.destroyBuffer(buffer, allocation);
}

vk::ResultValue<Image> Image::create(const stl::string& _name, Device* _device, vk::ImageType _image_type, vk::Format _format, const vk::Extent3D& _extent, vk::ImageUsageFlags _usage, u32 _mip_count, vma::MemoryUsage _memory_usage, u32 _layer_count) {

	auto [result, image] = _device->allocator.createImage({
		.imageType = _image_type,
		.format = _format,
		.extent = _extent,
		.mipLevels = _mip_count,
		.arrayLayers = _layer_count,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = _usage,
		.sharingMode = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined,
	}, {
		.usage = _memory_usage,
	});

	if (!failed(result)) {
		_device->set_object_name(image.first, _name);
	}

	return vk::ResultValue<Image>(result, {
		.parent_device = _device,
		.image = image.first,
		.allocation = image.second,
		.usage = _usage,
		.memory_usage = _memory_usage,
		.name = _name,
		.type = _image_type,
		.format = _format,
		.extent = _extent,
		.layer_count = _layer_count,
		.mip_count = _mip_count,
	});
}

void Image::destroy() {
	parent_device->allocator.destroyImage(image, allocation);
}

vk::ResultValue<ImageView> ImageView::create(Image* _image, vk::ImageViewType _image_type, const vk::ImageSubresourceRange& _subresource_range) {

	auto [result, image_view] = _image->parent_device->device.createImageView({
		.image = _image->image,
		.viewType = _image_type,
		.format = _image->format,
		.subresourceRange = _subresource_range,
	});

	const auto name = stl::fmt("%s view", _image->name.c_str());

	if (!failed(result)) {
		_image->parent_device->set_object_name(image_view, name);
	}

	return vk::ResultValue<ImageView>(result, {
		.parent_image = _image,
		.image_view = image_view,
		.format = _image->format,
		.type = _image_type,
		.subresource_range = _subresource_range,
		.name = name
	});
}

void ImageView::destroy() {
	parent_image->parent_device->device.destroyImageView(image_view);
}
