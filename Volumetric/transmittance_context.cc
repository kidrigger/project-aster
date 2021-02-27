// =============================================
//  Volumetric: transmittance_context.cc
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "transmittance_context.h"

#include <renderdoc/renderdoc.h>
#include <optick/optick.h>

TransmittanceContext::TransmittanceContext(const Borrowed<PipelineFactory>& _pipeline_factory, const AtmosphereInfo& _atmos)
	: parent_factory{ _pipeline_factory } {

	vk::Result result;

	const auto& device = _pipeline_factory->parent_device;

	if (auto res = Image::create("Transmittance LUT", device, vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat, transmittance_lut_extent, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)) {
		lut = std::move(res.value());
	} else {
		ERROR(std::fmt("LUT Image could not be created\n|> %s", res.error().what())) THEN_CRASH(res.error().code());
	}

	if (auto res = ImageView::create(borrow(lut), vk::ImageViewType::e2D, {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.levelCount = 1,
		.layerCount = 1,
	})) {
		lut_view = std::move(res.value());
	} else {
		ERROR(std::fmt("LUT Image View could not be created\n|> %s", res.error().what())) THEN_CRASH(res.error().code());
	}

	tie(result, lut_sampler) = device->device.createSampler({
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.addressModeU = vk::SamplerAddressMode::eClampToEdge,
		.addressModeV = vk::SamplerAddressMode::eClampToEdge,
	});
	ERROR_IF(failed(result), "LUT Image Sampler could not be created");
	device->set_object_name(lut_sampler, lut.name + " sampler");

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
	if (auto res = RenderPass::create("Transmittance LUT pass", _pipeline_factory->parent_device, {
		.attachmentCount = 1,
		.pAttachments = &attach_desc,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	})) {
		renderpass = std::move(res.value());
		INFO(std::fmt("Renderpass %s Created", renderpass.name.c_str()));
	} else {
		ERROR(std::fmt("Transmittance LUT pass creation failed" CODE_LOC "\n|> %s", res.error().what())) THEN_CRASH(res.error().code());
	}

	// Framebuffer
	tie(result, framebuffer) = device->device.createFramebuffer({
		.renderPass = renderpass.renderpass,
		.attachmentCount = 1,
		.pAttachments = &lut_view.image_view,
		.width = lut.extent.width,
		.height = lut.extent.height,
		.layers = 1,
	});
	ERROR_IF(failed(result), std::fmt("LUT Framebuffer creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("Framebuffer created");
	device->set_object_name(framebuffer, "Transmittance LUT Framebuffer");

	if (auto res = parent_factory->create_pipeline({
		.renderpass = borrow(renderpass),
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
	})) {
		pipeline = res.value();
		INFO("LUT Pipeline Created");
	} else {
		auto result_ = res.error().code();
		ERROR(std::fmt("LUT Pipeline creation failed with %s" CODE_LOC "\n|> %s", to_cstr(result_), res.error().what())) THEN_CRASH(result_);
	}

	recalculate(_atmos);
}

void TransmittanceContext::recalculate(const AtmosphereInfo& _atmos) {
	OPTICK_EVENT("Recalculate Transmittance");

	rdoc::start_capture();
	auto& device = parent_factory->parent_device;
	auto cmd = device->alloc_temp_command_buffer(device->graphics_cmd_pool);
	ERROR_IF(!cmd, std::fmt("Command buffer begin failed\n|> %s", cmd.error().what())) THEN_CRASH(cmd.error().code()) ELSE_INFO("Cmd Created");

	auto result = cmd->begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, });
	ERROR_IF(failed(result), std::fmt("Command buffer begin failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("Cmd Created");

	cmd->beginDebugUtilsLabelEXT({
		.pLabelName = "Transmittance LUT Calculation",
		.color = std::array{ 0.5f, 0.0f, 0.0f, 1.0f },
	});

	vk::ClearValue clear_val(std::array{ 0.0f, 1.0f, 0.0f, 1.0f });
	cmd->beginRenderPass({
		.renderPass = renderpass.renderpass,
		.framebuffer = framebuffer,
		.renderArea = {
			.offset = { 0, 0 },
			.extent = { lut.extent.width, lut.extent.height },
		},
		.clearValueCount = 1,
		.pClearValues = &clear_val,
	}, vk::SubpassContents::eInline);

	cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->pipeline);
	cmd->pushConstants(pipeline->layout->layout, vk::ShaderStageFlagBits::eFragment, 0u, vk::ArrayProxy<const AtmosphereInfo>{ _atmos });
	cmd->draw(4, 1, 0, 0);

	cmd->endRenderPass();

	cmd->endDebugUtilsLabelEXT();

	result = cmd->end();
	ERROR_IF(failed(result), std::fmt("Command buffer end failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("Command buffer Created");

	auto res = SubmitTask<void>::create(device, device->queues.graphics, device->graphics_cmd_pool, { cmd.value() })
	.map(&SubmitTask<void>::wait_and_destroy);
	ERROR_IF(!res, std::fmt("Submit failed\n|> %s", res.error().what())) THEN_CRASH(res.error().code()) ELSE_INFO("LUT Submitted Created");

	rdoc::end_capture();
}

TransmittanceContext::~TransmittanceContext() {
	auto& device = parent_factory->parent_device;

	device->device.destroySampler(lut_sampler);

	pipeline->destroy();
	device->device.destroyFramebuffer(framebuffer);
}
