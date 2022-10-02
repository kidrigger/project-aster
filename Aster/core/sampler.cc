// =============================================
//  Aster: sampler.cc
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#include "sampler.h"
#include <core/device.h>

Result<Sampler, vk::Result> Sampler::create(const std::string_view& _name, Device* _device, const vk::SamplerCreateInfo& _create_info) {
	auto [result, sampler] = _device->device.createSampler(_create_info);
	if (failed(result)) {
		return make_error(result);
	}

	_device->set_object_name(sampler, _name);
	return Sampler{
		.parent_device = _device,
		.sampler = sampler,
		.filter = { _create_info.minFilter, _create_info.magFilter },
		.mipmap_mode = _create_info.mipmapMode,
		.address_mode = { .u = _create_info.addressModeU, .v = _create_info.addressModeV, .w = _create_info.addressModeW },
		.mip_lod_bias = _create_info.mipLodBias,
		.anisotropy_enable = cast<b8>(_create_info.anisotropyEnable),
		.max_anisotropy = _create_info.maxAnisotropy,
		.compare_enable = cast<b8>(_create_info.compareEnable),
		.compare_op = _create_info.compareOp,
		.lod = { .min = _create_info.minLod, .max = _create_info.maxLod },
		.border_color = _create_info.borderColor,
		.unnormalized_coordinates = cast<b8>(_create_info.unnormalizedCoordinates),
		.name = name_t::from(_name),
	};
}

void Sampler::destroy() {
	if (parent_device && sampler) {
		parent_device->device.destroySampler(sampler);
		parent_device = nullptr;
		sampler = nullptr;
	}
}
