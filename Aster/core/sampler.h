// =============================================
//  Aster: sampler.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>

class Device;

struct Sampler {
	Borrowed<Device> parent_device;
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

	Sampler() = default;
	Sampler(const std::string_view& _name, const Borrowed<Device>& _device, vk::Sampler _sampler, const vk::SamplerCreateInfo& _create_info);

	Sampler(const Sampler& _other) = delete;
	Sampler(Sampler&& _other) noexcept;
	Sampler& operator=(const Sampler& _other) = delete;
	Sampler& operator=(Sampler&& _other) noexcept;

	~Sampler();

	static Res<Sampler> create(const std::string_view& _name, const Borrowed<Device>& _device, const vk::SamplerCreateInfo& _create_info);
};
