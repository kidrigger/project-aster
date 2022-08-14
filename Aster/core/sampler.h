// =============================================
//  Aster: sampler.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>

class Device;

struct Sampler {
	Device* parent_device;
	vk::Sampler sampler;

	struct {
		vk::Filter min{ vk::Filter::eNearest };
		vk::Filter mag{ vk::Filter::eNearest };
	} filter;

	vk::SamplerMipmapMode mipmap_mode{ vk::SamplerMipmapMode::eNearest };

	struct {
		vk::SamplerAddressMode u{ vk::SamplerAddressMode::eRepeat };
		vk::SamplerAddressMode v{ vk::SamplerAddressMode::eRepeat };
		vk::SamplerAddressMode w{ vk::SamplerAddressMode::eRepeat };
	} address_mode;

	f32 mip_lod_bias{};
	b8 anisotropy_enable{};
	f32 max_anisotropy{};
	b8 compare_enable{};
	vk::CompareOp compare_op{ vk::CompareOp::eNever };

	struct {
		f32 min{};
		f32 max{};
	} lod;

	vk::BorderColor border_color{ vk::BorderColor::eFloatTransparentBlack };
	b8 unnormalized_coordinates = {};
	std::string name;

	static vk::ResultValue<Sampler> create(const std::string_view& _name, Device* _device, const vk::SamplerCreateInfo& _create_info);

	void destroy();
};
