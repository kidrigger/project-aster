// =============================================
//  Volumetric: transmittance_context.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>

#include <core/pipeline.h>
#include <atmosphere_info.h>


#include "core/framebuffer.h"
#include "core/image.h"
#include "core/image_view.h"
#include "core/sampler.h"

struct TransmittanceContext {
	static constexpr vk::Extent3D transmittance_lut_extent = { 64, 256, 1 };

	TransmittanceContext(PipelineFactory* _pipeline_factory, const AtmosphereInfo& _atmos);

	TransmittanceContext(const TransmittanceContext& _other) = delete;
	TransmittanceContext(TransmittanceContext&& _other) = delete;
	TransmittanceContext& operator=(const TransmittanceContext& _other) = delete;
	TransmittanceContext& operator=(TransmittanceContext&& _other) = delete;

	~TransmittanceContext();

	void recalculate(PipelineFactory* _pipeline_factory, const AtmosphereInfo& _atmos);

	// fields

	Pipeline* pipeline{};
	RenderPass renderpass;
	Framebuffer framebuffer;

	Image lut;
	ImageView lut_view;
	Sampler lut_sampler;

	PipelineFactory* parent_factory;
};
