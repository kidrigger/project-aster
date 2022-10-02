// =============================================
//  Aster: resource_pool.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once
#include <deque>
#include <core/device.h>
#include <core/pipeline.h>

struct ResourcePool;

struct ResourceSet final {
	ResourcePool* parent_pool{ nullptr };
	ShaderInfo const* shader_info{ nullptr };
	std::vector<vk::DescriptorSet> sets;

	ResourceSet() = default;

	explicit ResourceSet(ResourcePool* _pool, ShaderInfo const* _shader_info, std::vector<vk::DescriptorSet>&& _sets)
		: parent_pool(_pool)
		, shader_info(_shader_info)
		, sets(std::move(_sets)) {}

	void set_buffer(const std::string_view& _name, vk::DescriptorBufferInfo&& _buffer_info);
	void set_buffer_array_index(const std::string_view& _name, vk::DescriptorBufferInfo&& _buffer_info, u32 _index);
	void set_buffer_array(const std::string_view& _name, const std::vector<vk::DescriptorBufferInfo>& _buffer_info, u32 _offset = 0);
	void set_texture(const std::string_view& _name, vk::DescriptorImageInfo&& _image_info);
	void set_texture_array_index(const std::string_view& _name, vk::DescriptorImageInfo&& _image_info, u32 _index);
	void set_texture_array(const std::string_view& _name, const std::vector<vk::DescriptorImageInfo>& _image_info, u32 _offset = 0);

	void update();

	void destroy() {}

	~ResourceSet() = default;

private:
	std::deque<vk::DescriptorBufferInfo> buffer_info_storage_{};
	std::deque<vk::DescriptorImageInfo> image_info_storage_{};
	std::vector<vk::WriteDescriptorSet> writes_{};
};

struct ResourcePool {
public:
	Device* parent_device;
	Layout* layout{ nullptr };
	vk::DescriptorPool descriptor_pool;
	u32 max_resource_sets{ 1 };

	Result<ResourceSet, vk::Result> allocate_resource_set();

	static Result<ResourcePool, vk::Result> create(Device* _device, Layout* _layout, u32 _max_resource_sets);

	void destroy() const {
		parent_device->device.destroyDescriptorPool(descriptor_pool);
	}
};
