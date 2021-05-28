// =============================================
//  Volumetric: sky_view_context.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>

#include <core/pipeline.h>
#include <core/image.h>
#include <core/image_view.h>
#include <core/camera.h>
#include <core/resource_pool.h>
#include <core/framebuffer.h>

#include <sun_data.h>
#include <transmittance_context.h>

#include <util/buffer_writer.h>

struct SkyViewContext {
	static constexpr vk::Extent3D sky_view_lut_extent = { 256, 128, 1 };

	SkyViewContext(const Borrowed<PipelineFactory>& _pipeline_factory, const Borrowed<TransmittanceContext>& _transmittance);

	SkyViewContext(const SkyViewContext& _other) = delete;
	SkyViewContext(SkyViewContext&& _other) = delete;
	SkyViewContext& operator=(const SkyViewContext& _other) = delete;
	SkyViewContext& operator=(SkyViewContext&& _other) = delete;

	~SkyViewContext();

	void update(const Camera& _camera, const SunData& _sun_data, const AtmosphereInfo& _atmos);
	void recalculate(vk::CommandBuffer _cmd);

	// Fields
	Pipeline* pipeline{};
	RenderPass renderpass;
	Framebuffer framebuffer;

	ResourcePool resource_pool;
	ResourceSet resource_set;
	
	//vk::DescriptorPool descriptor_pool;
	//vk::DescriptorSet descriptor_set;
	Buffer ubo;
	BufferWriter ubo_writer;

	Image lut;
	ImageView lut_view;

	// Borrowed
	Borrowed<TransmittanceContext> transmittance;

	Borrowed<PipelineFactory> parent_factory;
};
