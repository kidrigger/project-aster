// =============================================
//  Volumetric: sky_view_context.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>

#include <core/pipeline.h>

#include <sun_data.h>
#include <transmittance_context.h>

#include <core/camera.h>

struct SkyViewContext {
	static constexpr vk::Extent3D sky_view_lut_extent = { 192, 108, 1 };

	SkyViewContext(PipelineFactory* _pipeline_factory, TransmittanceContext* _transmittance);

	SkyViewContext(const SkyViewContext& _other) = delete;
	SkyViewContext(SkyViewContext&& _other) = delete;
	SkyViewContext& operator=(const SkyViewContext& _other) = delete;
	SkyViewContext& operator=(SkyViewContext&& _other) = delete;

	~SkyViewContext();

	void update(Camera* _camera, SunData* _sun_data, AtmosphereInfo* _atmos);
	void recalculate(vk::CommandBuffer _cmd);

	// Fields
	Pipeline* pipeline{};
	RenderPass renderpass;
	vk::Framebuffer framebuffer;
	vk::DescriptorPool descriptor_pool;
	vk::DescriptorSet descriptor_set;
	Buffer ubo;

	Image lut;
	ImageView lut_view;

	// Borrowed
	TransmittanceContext* transmittance;

	PipelineFactory* parent_factory;
};
