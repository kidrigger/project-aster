// =============================================
//  Aster: resource_pool.cc
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#include "resource_pool.h"

void ResourceSet::set_buffer(const std::string_view& _name, vk::DescriptorBufferInfo&& _buffer_info) {
	const auto& descriptor_info = shader_info->descriptors[shader_info->descriptor_names.find(std::string(_name))->second];

	buffer_info_storage_.push_back(std::move(_buffer_info));

	writes_.push_back({
		.dstSet = sets[descriptor_info.set],
		.dstBinding = descriptor_info.binding,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = descriptor_info.type,
		.pBufferInfo = &buffer_info_storage_.back(),
	});
}

void ResourceSet::set_buffer_array_index(const std::string_view& _name, vk::DescriptorBufferInfo&& _buffer_info, const u32 _index) {
	const auto& descriptor_info = shader_info->descriptors[shader_info->descriptor_names.find(std::string(_name))->second];

	buffer_info_storage_.push_back(std::move(_buffer_info));

	writes_.push_back({
		.dstSet = sets[descriptor_info.set],
		.dstBinding = descriptor_info.binding,
		.dstArrayElement = _index,
		.descriptorCount = 1,
		.descriptorType = descriptor_info.type,
		.pBufferInfo = &buffer_info_storage_.back(),
	});
}

void ResourceSet::set_buffer_array(const std::string_view& _name, const std::vector<vk::DescriptorBufferInfo>& _buffer_info, const u32 _offset) {
	const auto& descriptor_info = shader_info->descriptors[shader_info->descriptor_names.find(std::string(_name))->second];

	const auto head = buffer_info_storage_.insert(buffer_info_storage_.end(), _buffer_info.begin(), _buffer_info.end());

	writes_.push_back({
		.dstSet = sets[descriptor_info.set],
		.dstBinding = descriptor_info.binding,
		.dstArrayElement = _offset,
		.descriptorCount = cast<u32>(_buffer_info.size()),
		.descriptorType = descriptor_info.type,
		.pBufferInfo = &*head,
	});
}

void ResourceSet::set_texture(const std::string_view& _name, vk::DescriptorImageInfo&& _image_info) {
	const auto& descriptor_info = shader_info->descriptors[shader_info->descriptor_names.find(std::string(_name))->second];

	image_info_storage_.push_back(std::move(_image_info));

	writes_.push_back({
		.dstSet = sets[descriptor_info.set],
		.dstBinding = descriptor_info.binding,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = descriptor_info.type,
		.pImageInfo = &image_info_storage_.back(),
	});
}

void ResourceSet::set_texture_array_index(const std::string_view& _name, vk::DescriptorImageInfo&& _image_info, const u32 _index) {
	const auto& descriptor_info = shader_info->descriptors[shader_info->descriptor_names.find(std::string(_name))->second];

	image_info_storage_.push_back(std::move(_image_info));

	writes_.push_back({
		.dstSet = sets[descriptor_info.set],
		.dstBinding = descriptor_info.binding,
		.dstArrayElement = _index,
		.descriptorCount = 1,
		.descriptorType = descriptor_info.type,
		.pImageInfo = &image_info_storage_.back(),
	});
}

void ResourceSet::set_texture_array(const std::string_view& _name, const std::vector<vk::DescriptorImageInfo>& _image_info, u32 _offset) {
	const auto& descriptor_info = shader_info->descriptors[shader_info->descriptor_names.find(std::string(_name))->second];

	const auto head = image_info_storage_.insert(image_info_storage_.end(), _image_info.begin(), _image_info.end());

	writes_.push_back({
		.dstSet = sets[descriptor_info.set],
		.dstBinding = descriptor_info.binding,
		.dstArrayElement = _offset,
		.descriptorCount = cast<u32>(_image_info.size()),
		.descriptorType = descriptor_info.type,
		.pImageInfo = &*head,
	});
}

void ResourceSet::update() {
	parent_pool->parent_device->device.updateDescriptorSets(writes_, {});

	writes_.clear();
	buffer_info_storage_.clear();
	image_info_storage_.clear();
}

Result<ResourceSet, vk::Result> ResourcePool::allocate_resource_set() {
	std::vector<vk::DescriptorSet> sets;
	auto [res, set_vector] = parent_device->device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = cast<u32>(layout->descriptor_set_layouts.size()),
		.pSetLayouts = layout->descriptor_set_layouts.data()
	});
	if (failed(res)) {
		ERROR("Layout %s Descriptor Set allocation failed");
		return make_error(res);
	}
	return std::move(ResourceSet{ this, &layout->layout_info, std::move(set_vector) });
}

Result<ResourcePool, vk::Result> ResourcePool::create(Device* _device, Layout* _layout, const u32 _max_resource_sets) {

	auto& shader_info = _layout->layout_info;

	std::vector<vk::DescriptorPoolSize> pool_sizes;
	for (auto& desc_ : shader_info.descriptors) {
		for (auto& pool_ : pool_sizes) {
			if (pool_.type == desc_.type) {
				++pool_.descriptorCount;
				break;
			}
		}
		pool_sizes.push_back(vk::DescriptorPoolSize{
			.type = desc_.type,
			.descriptorCount = 1,
		});
	}

	const auto descriptor_set_count = cast<u32>(_layout->descriptor_set_layouts.size());

	vk::DescriptorPool pool;
	if (const auto res = _device->device.createDescriptorPool(vk::DescriptorPoolCreateInfo{
		.maxSets = _max_resource_sets * descriptor_set_count,
		.poolSizeCount = cast<u32>(pool_sizes.size()),
		.pPoolSizes = pool_sizes.data()
	}); failed(res.result)) {
		return make_error(res.result);
	} else {
		pool = res.value;
	}

	return ResourcePool{ _device, _layout, pool, _max_resource_sets };
}
