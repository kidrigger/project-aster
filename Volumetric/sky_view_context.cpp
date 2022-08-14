// =============================================
//  Volumetric: sky_view_context.cpp
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "sky_view_context.h"


#include "core/image.h"
#include "core/image_view.h"
#include "optick/optick.h"

SkyViewContext::SkyViewContext(PipelineFactory* _pipeline_factory, TransmittanceContext* _transmittance)
	: parent_factory{ _pipeline_factory } {

	vk::Result result;
	auto* const device = _pipeline_factory->parent_device;
	const auto ubo_alignment = device->physical_device_properties.limits.minUniformBufferOffsetAlignment;

	tie(result, ubo) = Buffer::create("Sky View uniform buffer", device, closest_multiple(sizeof(Camera), ubo_alignment) + closest_multiple(sizeof(SunData), ubo_alignment) + closest_multiple(sizeof(AtmosphereInfo), ubo_alignment), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);
	ERROR_IF(failed(result), std::fmt("Skyview UBO creation failed with %s", to_cstr(result)));

	ubo_writer = BufferWriter{ &ubo };

	transmittance = _transmittance;

	tie(result, lut) = Image::create("Sky View LUT", device, vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat, sky_view_lut_extent, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
	ERROR_IF(failed(result), std::fmt("Skyview LUT creation failed with %s", to_cstr(result)));

	tie(result, lut_view) = ImageView::create(&lut, vk::ImageViewType::e2D, {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.levelCount = 1,
		.layerCount = 1,
	});
	ERROR_IF(failed(result), std::fmt("Skyview LUT view creation failed with %s", to_cstr(result)));

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
	tie(result, renderpass) = RenderPass::create("Skyview LUT pass", _pipeline_factory->parent_device, {
		.attachmentCount = 1,
		.pAttachments = &attach_desc,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency,
	});
	ERROR_IF(failed(result), std::fmt("Renderpass %s creation failed with %s", renderpass.name.c_str(), to_cstr(result)));

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
		.shader_files = { R"(res/shaders/sky_view_lut.vs.spv)", R"(res/shaders/sky_view_lut.fs.spv)" },
		.name = "Sky View LUT Pipeline",
	});
	ERROR_IF(failed(result), std::fmt("Skyview LUT Pipeline creation failed with %s", to_cstr(result)));

	tie(result, framebuffer) = Framebuffer::create("Skyview LUT FB", &renderpass, { lut_view }, 1);
	ERROR_IF(failed(result), std::fmt("Skyview LUT Framebuffer creation failed with %s", to_cstr(result)));

	std::vector<vk::DescriptorPoolSize> pool_sizes = {
		{
			.type = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 3,
		},
		{
			.type = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 1,
		}
	};

	tie(result, descriptor_pool) = device->device.createDescriptorPool({
		.maxSets = 1,
		.poolSizeCount = cast<u32>(pool_sizes.size()),
		.pPoolSizes = pool_sizes.data(),
	});
	ERROR_IF(failed(result), std::fmt("Skyview LUT Descriptor pool creation failed with %s", to_cstr(result)));

	std::vector<vk::DescriptorSet> descriptor_sets;
	const auto& layout = pipeline->layout->descriptor_set_layouts.front();
	tie(result, descriptor_sets) = device->device.allocateDescriptorSets({
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	});
	descriptor_set = descriptor_sets.front();
	ERROR_IF(failed(result), std::fmt("Skyview LUT Descriptor creation failed with %s", to_cstr(result)));

	{
		usize cam_offset, sun_offset, atmos_offset;
		usize offset = 0;
		cam_offset = offset;
		offset += closest_multiple(sizeof(Camera), ubo_alignment);
		sun_offset = offset;
		offset += closest_multiple(sizeof(SunData), ubo_alignment);
		atmos_offset = offset;

		std::vector<vk::DescriptorBufferInfo> buf_info = {
			{
				.buffer = ubo.buffer,
				.offset = cam_offset,
				.range = sizeof(Camera),
			},
			{
				.buffer = ubo.buffer,
				.offset = sun_offset,
				.range = sizeof(SunData),
			},
			{
				.buffer = ubo.buffer,
				.offset = atmos_offset,
				.range = sizeof(AtmosphereInfo),
			}
		};

		vk::DescriptorImageInfo image_info = {
			.sampler = transmittance->lut_sampler.sampler,
			.imageView = transmittance->lut_view.image_view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		vk::WriteDescriptorSet wd0 = {
			.dstSet = descriptor_set,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &buf_info[0],
		};

		vk::WriteDescriptorSet wd1 = {
			.dstSet = descriptor_set,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &buf_info[1],
		};

		vk::WriteDescriptorSet wd2 = {
			.dstSet = descriptor_set,
			.dstBinding = 2,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &buf_info[2],
		};

		vk::WriteDescriptorSet wd3 = {
			.dstSet = descriptor_set,
			.dstBinding = 3,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &image_info,
		};

		device->device.updateDescriptorSets({ wd0, wd1, wd2, wd3 }, {});
	}
}

void SkyViewContext::update(const Camera& _camera, const SunData& _sun_data, const AtmosphereInfo& _atmos) {
	ubo_writer << _camera << _sun_data << _atmos;
}

void SkyViewContext::recalculate(vk::CommandBuffer _cmd) {

	OPTICK_EVENT("Recalculate Skyview");
	_cmd.beginDebugUtilsLabelEXT({
		.pLabelName = "Sky View LUT Calculation",
		.color = std::array{ 0.1f, 0.0f, 0.5f, 1.0f },
	});

	vk::ClearValue clear_val(std::array{ 0.0f, 1.0f, 0.0f, 1.0f });
	_cmd.beginRenderPass({
		.renderPass = renderpass.renderpass,
		.framebuffer = framebuffer.framebuffer,
		.renderArea = {
			.offset = { 0, 0 },
			.extent = { lut.extent.width, lut.extent.height },
		},
		.clearValueCount = 1,
		.pClearValues = &clear_val,
	}, vk::SubpassContents::eInline);

	_cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->pipeline);
	_cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->layout->layout, 0, { descriptor_set }, {});
	_cmd.draw(4, 1, 0, 0);

	_cmd.endRenderPass();

	_cmd.endDebugUtilsLabelEXT();
}

SkyViewContext::~SkyViewContext() {
	auto* const device = parent_factory->parent_device;
	device->device.destroyDescriptorPool(descriptor_pool);

	ubo.destroy();

	framebuffer.destroy();
	pipeline->destroy();
	renderpass.destroy();

	lut_view.destroy();
	lut.destroy();
}
