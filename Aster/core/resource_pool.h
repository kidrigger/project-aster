// =============================================
//  Aster: descriptor_allocator.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once
#include <deque>
#include <core/device.h>
#include <core/pipeline.h>

class ResourcePool;

struct ResourceSet {
public:
	Borrowed<ResourcePool> parent_pool;
	Borrowed<ShaderInfo> shader_info;
	std::vector<vk::DescriptorSet> sets;

	ResourceSet() = default;

	explicit ResourceSet(const Borrowed<ResourcePool>& _pool, const Borrowed<ShaderInfo>& _shader_info, std::vector<vk::DescriptorSet>&& _sets)
		: parent_pool(_pool)
		, shader_info(_shader_info)
		, sets(std::move(_sets)) {}

	ResourceSet(const ResourceSet& _other) = delete;

	ResourceSet(ResourceSet&& _other) noexcept
		: parent_pool{ std::move(_other.parent_pool) }
		, shader_info{ std::move(_other.shader_info) }
		, sets{ std::move(_other.sets) }
		, buffer_info_storage_{ std::move(_other.buffer_info_storage_) }
		, image_info_storage_{ std::move(_other.image_info_storage_) }
		, writes_{ std::move(_other.writes_) } {}

	ResourceSet& operator=(const ResourceSet& _other) = delete;

	ResourceSet& operator=(ResourceSet&& _other) noexcept {
		if (this == &_other) return *this;
		parent_pool = std::move(_other.parent_pool);
		shader_info = std::move(_other.shader_info);
		sets = std::move(_other.sets);
		buffer_info_storage_ = std::move(_other.buffer_info_storage_);
		image_info_storage_ = std::move(_other.image_info_storage_);
		writes_ = std::move(_other.writes_);
		return *this;
	}

	void set_buffer(const std::string_view& _name, vk::DescriptorBufferInfo&& _buffer_info);
	void set_buffer_array_index(const std::string_view& _name, vk::DescriptorBufferInfo&& _buffer_info, u32 _index);
	void set_buffer_array(const std::string_view& _name, const std::vector<vk::DescriptorBufferInfo>& _buffer_info, u32 _offset = 0);
	void set_texture(const std::string_view& _name, vk::DescriptorImageInfo&& _image_info);
	void set_texture_array_index(const std::string_view& _name, vk::DescriptorImageInfo&& _image_info, u32 _index);
	void set_texture_array(const std::string_view& _name, const std::vector<vk::DescriptorImageInfo>& _image_info, u32 _offset = 0);

	void update();

	~ResourceSet() = default;

private:
	std::deque<vk::DescriptorBufferInfo> buffer_info_storage_;
	std::deque<vk::DescriptorImageInfo> image_info_storage_;
	std::vector<vk::WriteDescriptorSet> writes_;
};

class ResourcePool {
public:
	Borrowed<Device> parent_device;
	Layout* layout{ nullptr };
	u32 max_resource_sets{ 1 };

	ResourcePool() = default;

	ResourcePool(const Borrowed<Device>& _parent_device, Layout* _layout, const vk::DescriptorPool& _pool, u32 _max_resource_sets)
		: parent_device(_parent_device)
		, layout(_layout)
		, max_resource_sets(_max_resource_sets)
		, pool_(_pool) {}


	ResourcePool(const ResourcePool& _other) = delete;

	ResourcePool(ResourcePool&& _other) noexcept
		: parent_device{ std::move(_other.parent_device) }
		, layout{ std::exchange(_other.layout, nullptr) }
		, pool_{ std::exchange(_other.pool_, nullptr) } {}

	ResourcePool& operator=(const ResourcePool& _other) = delete;

	ResourcePool& operator=(ResourcePool&& _other) noexcept {
		if (this == &_other) return *this;
		parent_device = std::move(_other.parent_device);
		layout = std::exchange(_other.layout, nullptr);
		pool_ = std::exchange(_other.pool_, nullptr);
		return *this;
	}

	Res<ResourceSet> allocate_resource_set();

	static Res<ResourcePool> create(const Borrowed<Device>& _device, Layout* _layout, u32 _max_resource_sets);

	~ResourcePool() {
		parent_device->device.destroyDescriptorPool(pool_);
	}

private:
	vk::DescriptorPool pool_;
};
