// =============================================
//  Volumetric: transmittance_context.cc
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "transmittance_context.h"

#include <renderdoc/renderdoc.h>
#include <optick/optick.h>
#include <core/image.h>
#include <core/image_view.h>

TransmittanceContext::TransmittanceContext(PipelineFactory* _pipeline_factory, const AtmosphereInfo& _atmos)
	: parent_factory{ _pipeline_factory } {

	vk::Result result;

	auto* device = _pipeline_factory->parent_device;
	const auto ubo_alignment = device->physical_device_properties.limits.minUniformBufferOffsetAlignment;

	tie(result, lut) = Image::create("Transmittance LUT", device, vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat, transmittance_lut_extent, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
	ERROR_IF(failed(result), std::fmt("LUT Image could not be created with %s", to_cstr(result)));

	tie(result, lut_view) = ImageView::create(&lut, vk::ImageViewType::e2D, {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.levelCount = 1,
		.layerCount = 1,
	});
	ERROR_IF(failed(result), "LUT Image View could not be created");

	tie(result, lut_sampler) = Sampler::create("LUT Image Sampler", device, {
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.addressModeU = vk::SamplerAddressMode::eClampToEdge,
		.addressModeV = vk::SamplerAddressMode::eClampToEdge,
	});
	ERROR_IF(failed(result), "LUT Image Sampler could not be created");

	vk::AttachmentDescription attach_desc = {
		.format = lut.format,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
	};

	vk::AttachmentReference attach_ref = {
		.attachment = 0,
		.layout = vk::ImageLayout::eColorAttachmentOptimal,
	};

	vk::SubpassDescription subpass = {
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.colorAttachmentCount = 1,
		.pColorAttachments = &attach_ref,
	};

	vk::SubpassDependency dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
		.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
		.srcAccessMask = {},
		.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
	};

	// Renderpass
	tie(result, renderpass) = RenderPass::create("Transmittance LUT pass", _pipeline_factory->parent_device, {
		.attachmentCount = 1,
		.pAttachments = &attach_desc,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	});
	ERROR_IF(failed(result), std::fmt("Renderpass %s creation failed with %s", renderpass.name.c_str(), to_cstr(result))) THEN_CRASH(result) ELSE_INFO(std::fmt("Renderpass %s Created", renderpass.name.c_str()));

	// Framebuffer
	tie(result, framebuffer) = Framebuffer::create("LUT Framebuffer", &renderpass, { lut_view }, 1);
	ERROR_IF(failed(result), std::fmt("LUT Framebuffer creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("Framebuffer created");

	tie(result, pipeline) = _pipeline_factory->create_pipeline({
		.renderpass = renderpass,
		.viewport_state = {
			.enable_dynamic = false,
			.viewports = {
				{
					.x = 0.0f,
					.y = 0.0f,
					.width = cast<f32>(lut.extent.width),
					.height = cast<f32>(lut.extent.height),
					.minDepth = 0.0f,
					.maxDepth = 1.0f,
				}
			},
			.scissors = {
				{
					.offset = { 0, 0 },
					.extent = { lut.extent.width, lut.extent.height },
				}
			}
		},
		.raster_state = {
			.front_face = vk::FrontFace::eCounterClockwise,
		},
		.shader_files = { R"(res/shaders/transmittance_lut.vs.spv)", R"(res/shaders/transmittance_lut.fs.spv)" },
		.name = "LUT Pipeline",
	});
	ERROR_IF(failed(result), std::fmt("LUT Pipeline creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("LUT Pipeline Created");

	recalculate(_pipeline_factory, _atmos);
}

void TransmittanceContext::recalculate(PipelineFactory* _pipeline_factory, const AtmosphereInfo& _atmos) {
	OPTICK_EVENT("Recalculate Transmittance");

	rdoc::start_capture();
	auto* device = _pipeline_factory->parent_device;
	auto [result, cmd] = device->alloc_temp_command_buffer(device->graphics_cmd_pool);

	result = cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, });
	ERROR_IF(failed(result), std::fmt("Command buffer begin failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("Cmd Created");

	cmd.beginDebugUtilsLabelEXT({
		.pLabelName = "Transmittance LUT Calculation",
		.color = std::array{ 0.5f, 0.0f, 0.0f, 1.0f },
	});

	vk::ClearValue clear_val(std::array{ 0.0f, 1.0f, 0.0f, 1.0f });
	cmd.beginRenderPass({
		.renderPass = renderpass.renderpass,
		.framebuffer = framebuffer.framebuffer,
		.renderArea = {
			.offset = { 0, 0 },
			.extent = { lut.extent.width, lut.extent.height },
		},
		.clearValueCount = 1,
		.pClearValues = &clear_val,
	}, vk::SubpassContents::eInline);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->pipeline);
	cmd.pushConstants(pipeline->layout->layout, vk::ShaderStageFlagBits::eFragment, 0u, vk::ArrayProxy<const AtmosphereInfo>{ _atmos });
	cmd.draw(4, 1, 0, 0);

	cmd.endRenderPass();

	cmd.endDebugUtilsLabelEXT();

	result = cmd.end();
	ERROR_IF(failed(result), std::fmt("Command buffer end failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("Command buffer Created");

	SubmitTask<void> task;
	result = task.submit(device, device->queues.graphics, device->graphics_cmd_pool, { cmd });
	ERROR_IF(failed(result), std::fmt("Submit failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("LUT Submitted Created");

	result = task.wait_and_destroy();
	ERROR_IF(failed(result), std::fmt("Fence waiting failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("LUT Written to");

	rdoc::end_capture();
}

TransmittanceContext::~TransmittanceContext() {
	lut.destroy();
	lut_view.destroy();
	lut_sampler.destroy();

	pipeline->destroy();
	framebuffer.destroy();
	renderpass.destroy();
}
