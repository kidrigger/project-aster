// =============================================
//  Volumetric: sky_view_context.cpp
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "sky_view_context.h"

#include "optick/optick.h"

SkyViewContext::SkyViewContext(const Borrowed<PipelineFactory>& _pipeline_factory, const Borrowed<TransmittanceContext>& _transmittance)
	: parent_factory{ _pipeline_factory } {

	const auto& device = _pipeline_factory->parent_device;
	const auto ubo_alignment = device->physical_device.properties.limits.minUniformBufferOffsetAlignment;

	ubo = Buffer::create("Sky View uniform buffer", device, closest_multiple(sizeof(Camera), ubo_alignment) + closest_multiple(sizeof(SunData), ubo_alignment) + closest_multiple(sizeof(AtmosphereInfo), ubo_alignment), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu).value();

	ubo_writer = BufferWriter{ borrow(ubo) };

	transmittance = _transmittance;

	lut = Image::create("Sky View LUT", device, vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat, sky_view_lut_extent, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled).value();

	lut_view = ImageView::create(borrow(lut), vk::ImageViewType::e2D, {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.levelCount = 1,
		.layerCount = 1,
		}).value();

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
	renderpass = RenderPass::create("Sky View LUT pass", _pipeline_factory->parent_device, {
		.attachmentCount = 1,
		.pAttachments = &attach_desc,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency,
		}).value();

	pipeline = parent_factory->create_pipeline({
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
		.shader_files = { R"(res/shaders/sky_view_lut.vs.spv)", R"(res/shaders/sky_view_lut.fs.spv)" },
		.name = "Sky View LUT Pipeline",
		}).value();

	framebuffer = Framebuffer::create("Sky view LUT framebuffer", borrow(renderpass), { borrow(lut_view) }, 1).value();

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

	resource_pool = ResourcePool::create(device, pipeline->layout, 1).value();
	resource_set = resource_pool.allocate_resource_set().value();

	{
		usize offset = 0;
		const auto cam_offset = offset;
		offset += closest_multiple(sizeof(Camera), ubo_alignment);
		const auto sun_offset = offset;
		offset += closest_multiple(sizeof(SunData), ubo_alignment);
		const auto atmos_offset = offset;

		resource_set.set_buffer("camera", {
				.buffer = ubo.buffer,
				.offset = cam_offset,
				.range = sizeof(Camera),
		});
		resource_set.set_buffer("sun",{
				.buffer = ubo.buffer,
				.offset = sun_offset,
				.range = sizeof(SunData),
		});
		resource_set.set_buffer("atmos", {
				.buffer = ubo.buffer,
				.offset = atmos_offset,
				.range = sizeof(AtmosphereInfo),
		});
		resource_set.set_texture("transmittance_lut", {
			.sampler = transmittance->lut_sampler.sampler,
			.imageView = transmittance->lut_view.image_view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		});
		resource_set.update();
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
			.extent = framebuffer.extent,
		},
		.clearValueCount = 1,
		.pClearValues = &clear_val,
	}, vk::SubpassContents::eInline);

	_cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->pipeline);
	_cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->layout->layout, 0, resource_set.sets, {});
	_cmd.draw(4, 1, 0, 0);

	_cmd.endRenderPass();

	_cmd.endDebugUtilsLabelEXT();
}

SkyViewContext::~SkyViewContext() {
	pipeline->destroy();
}
