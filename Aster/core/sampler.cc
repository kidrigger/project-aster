// =============================================
//  Aster: sampler.cc
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "sampler.h"
#include <core/device.h>

Sampler::Sampler(const std::string_view& _name, const Borrowed<Device>& _device, vk::Sampler _sampler, const vk::SamplerCreateInfo& _create_info)
	: parent_device{ _device }
	, sampler{ _sampler }
	, filter{ _create_info.minFilter, _create_info.magFilter }
	, mipmap_mode{ _create_info.mipmapMode }
	, address_mode{ .u = _create_info.addressModeU, .v = _create_info.addressModeV, .w = _create_info.addressModeW }
	, mip_lod_bias{ _create_info.mipLodBias }
	, anisotropy_enable{ cast<b8>(_create_info.anisotropyEnable) }
	, max_anisotropy{ _create_info.maxAnisotropy }
	, compare_enable{ cast<b8>(_create_info.compareEnable) }
	, compare_op{ _create_info.compareOp }
	, lod{ .min = _create_info.minLod, .max = _create_info.maxLod }
	, border_color{ _create_info.borderColor }
	, unnormalized_coordinates{ cast<b8>(_create_info.unnormalizedCoordinates) }
	, name{ _name } { }

Sampler::Sampler(Sampler&& _other) noexcept: parent_device{ std::move(_other.parent_device) }
                                           , sampler{ std::exchange(_other.sampler, nullptr) }
                                           , mipmap_mode{ _other.mipmap_mode }
                                           , mip_lod_bias{ _other.mip_lod_bias }
                                           , anisotropy_enable{ _other.anisotropy_enable }
                                           , max_anisotropy{ _other.max_anisotropy }
                                           , compare_enable{ _other.compare_enable }
                                           , compare_op{ _other.compare_op }
                                           , border_color{ _other.border_color }
                                           , unnormalized_coordinates{ _other.unnormalized_coordinates }
                                           , name{ std::move(_other.name) } {}

Sampler& Sampler::operator=(Sampler&& _other) noexcept {
	if (this == &_other) return *this;
	parent_device = std::move(_other.parent_device);
	sampler = std::exchange(_other.sampler, nullptr);
	mipmap_mode = _other.mipmap_mode;
	mip_lod_bias = _other.mip_lod_bias;
	anisotropy_enable = _other.anisotropy_enable;
	max_anisotropy = _other.max_anisotropy;
	compare_enable = _other.compare_enable;
	compare_op = _other.compare_op;
	border_color = _other.border_color;
	unnormalized_coordinates = _other.unnormalized_coordinates;
	name = std::move(_other.name);
	return *this;
}

Sampler::~Sampler() {
	if (parent_device && sampler) {
		parent_device->device.destroySampler(sampler);
	}
}

Res<Sampler> Sampler::create(const std::string_view& _name, const Borrowed<Device>& _device, const vk::SamplerCreateInfo& _create_info) {
	auto [result, sampler] = _device->device.createSampler(_create_info);
	if (failed(result)) {
		return Err::make(std::fmt("Sampler '%s' creation failed with %s" CODE_LOC, _name.data(), to_cstr(result)), result);
	}
	_device->set_object_name(sampler, _name);
	return Sampler{
		_name,
		_device,
		sampler,
		_create_info
	};
}
