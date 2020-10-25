/*=========================================*/
/*  Aster: source.cc                       */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include <global.h>
#include <logger.h>
#include <cstdio>

#include <core/glfw_context.h>
#include <core/window.h>
#include <core/context.h>
#include <core/device.h>
#include <core/swapchain.h>

#include <util/files.h>

#include <EASTL/vector.h>
#include <EASTL/vector_map.h>

struct Layout {
	vk::PipelineLayout pipeline_layout;
	stl::vector<vk::DescriptorSetLayout> descriptor_layout;
};

int main() {

	g_logger.set_minimum_logging_level(Logger::LogType::eInfo);

	GLFWContext glfw;
	Context context;
	Window window;
	Device device;
	Swapchain swapchain;

	glfw.init();
	context.init("Aster Core", Version{ 0, 0, 1 });
	window.init(&context, 640u, 480u, PROJECT_NAME, false);
	device.init("Primary", &context, &window);
	swapchain.init(window.name, &window, &device);

	vk::Result result;
	vk::PipelineLayout layout;
	stl::array<vk::ShaderModule, 2> shaders;
	vk::Pipeline pipeline;
	vk::RenderPass renderpass;
	stl::fixed_vector<vk::Framebuffer, 3> framebuffers;

	tie(result, layout) = device.device.createPipelineLayout({});
	ERROR_IF(failed(result), stl::fmt("Layout creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Pipeline Layout Created");

	// Shaders
	auto vertex_code = load_binary32_file(R"(shader.vs.spv)");
	tie(result, shaders[0]) = device.device.createShaderModule(vk::ShaderModuleCreateInfo{
		.codeSize = sizeof(u32) * vertex_code.size(),
		.pCode = vertex_code.data()
	});
	ERROR_IF(failed(result), stl::fmt("Vertex shader creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Vertex Shader Created");

	auto pixel_code = load_binary32_file(R"(shader.fs.spv)");
	tie(result, shaders[1]) = device.device.createShaderModule(vk::ShaderModuleCreateInfo{
		.codeSize = sizeof(u32) * pixel_code.size(),
		.pCode = pixel_code.data()
	});
	ERROR_IF(failed(result), stl::fmt("Fragment shader creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Frahment Shader Created");

	vk::AttachmentDescription attach_desc = {
		.format = swapchain.format,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::ePresentSrcKHR,
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
	tie(result, renderpass) = device.device.createRenderPass({
		.attachmentCount = 1,
		.pAttachments = &attach_desc,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency,
	});
	ERROR_IF(failed(result), stl::fmt("Renderpass creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Renderpass Created");

	for (u32 i = 0; i < swapchain.image_count; ++i) {
		tie(result, framebuffers.push_back()) = device.device.createFramebuffer({
			.renderPass = renderpass,
			.attachmentCount = 1,
			.pAttachments = &swapchain.image_views[i],
			.width = window.extent.width,
			.height = window.extent.height,
			.layers = 1,
		});
		ERROR_IF(failed(result), stl::fmt("Framebuffer creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Framebuffer Created");
	}

	struct Vertex {
		vec4 position;
		vec4 color;
	};

	stl::fixed_vector<vk::VertexInputAttributeDescription, 2> viad = {
		{
			.location = 0,
			.binding = 0,
			.format = vk::Format::eR32G32B32A32Sfloat,
			.offset = offsetof(Vertex, position),
		}, {
			.location = 1,
			.binding = 0,
			.format = vk::Format::eR32G32B32A32Sfloat,
			.offset = offsetof(Vertex, color),
		},
	};

	vk::VertexInputBindingDescription vibd = {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = vk::VertexInputRate::eVertex,
	};

	vk::PipelineVertexInputStateCreateInfo visci = {
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vibd,
		.vertexAttributeDescriptionCount = cast<u32>(viad.size()),
		.pVertexAttributeDescriptions = viad.data(),
	};

	vk::PipelineInputAssemblyStateCreateInfo iasci = {
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = false,
	};

	vk::Viewport viewport = {
		.x = 0,
		.y = 0,
		.width = cast<f32>(window.extent.width),
		.height = cast<f32>(window.extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	vk::Rect2D scissor = {
		.offset = {0, 0},
		.extent = window.extent,
	};

	vk::PipelineViewportStateCreateInfo vsci = {
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor,
	};

	vk::PipelineRasterizationStateCreateInfo rsci = {
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eNone,
		.lineWidth = 1.0f,
	};

	vk::PipelineMultisampleStateCreateInfo msci = {
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = false,
	};

	stl::vector<vk::PipelineShaderStageCreateInfo> ssci = {
		{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = shaders[0],
			.pName = "main",
		}, {
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = shaders[1],
			.pName = "main",
		},
	};

	vk::PipelineColorBlendAttachmentState cbas = {
		.blendEnable = false,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};

	vk::PipelineColorBlendStateCreateInfo cbsci = {
		.logicOpEnable = false,
		.attachmentCount = 1,
		.pAttachments = &cbas,
	};

	tie(result, pipeline) = device.device.createGraphicsPipeline({/*Cache*/}, {
		.stageCount = cast<u32>(ssci.size()),
		.pStages = ssci.data(),
		.pVertexInputState = &visci,
		.pInputAssemblyState = &iasci,
		.pViewportState = &vsci,
		.pRasterizationState = &rsci,
		.pMultisampleState = &msci,
		.pColorBlendState = &cbsci,
		.layout = layout,
		.renderPass = renderpass,
	});
	ERROR_IF(failed(result), stl::fmt("Pipeline creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Pipeline Created");

	vk::CommandPool command_pool;
	tie(result, command_pool) = device.device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = device.queue_families.graphics_idx,
	});

	// Vertices
	std::pair<vk::Buffer, vma::Allocation> buffer;
	tie(result, buffer) = device.allocator.createBuffer({
		.size = 4 * sizeof(Vertex),
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eVertexBuffer,
		.sharingMode = vk::SharingMode::eExclusive,
	}, {
		.usage = vma::MemoryUsage::eCpuToGpu,
	});
	device.set_object_name(buffer.first, "Vertex Buffer");
	{
		void* mapped_memory;
		tie(result, mapped_memory) = device.allocator.mapMemory(buffer.second);
		ERROR_IF(failed(result), stl::fmt("Memory mapping failed with %s", to_cstring(result))) THEN_CRASH(result);
		auto vertices = cast<Vertex*>(mapped_memory);
		vertices[0] = { vec4(0.0f, 0.5f, 0.5f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f) };
		vertices[1] = { vec4(0.5f, -0.5f, 0.5f, 1.0f), vec4(0.0f, 1.0f, 0.0f, 1.0f) };
		vertices[2] = { vec4(-0.5f, -0.5f, 0.5f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 1.0f) };
		vertices[3] = vertices[0];
		device.allocator.unmapMemory(buffer.second);
	};

	stl::fixed_vector<vk::Semaphore, 3> image_available_sem;
	stl::fixed_vector<vk::Semaphore, 3> render_finished_sem;
	stl::fixed_vector<vk::Fence, 3> in_flight_fence;

	for (u32 i = 0; i < swapchain.image_count; ++i) {
		tie(result, image_available_sem.push_back()) = device.device.createSemaphore({});
		tie(result, render_finished_sem.push_back()) = device.device.createSemaphore({});
		tie(result, in_flight_fence.push_back()) = device.device.createFence({
			.flags = vk::FenceCreateFlagBits::eSignaled,
		});
	}

	stl::vector<vk::CommandBuffer> command_buffers;
	{
		std::vector<vk::CommandBuffer> commands;
		tie(result, commands) = device.device.allocateCommandBuffers({
			.commandPool = command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = swapchain.image_count,
		});
		for (auto& buf : commands) {
			command_buffers.push_back(buf);
		}
	}

	u32 frame_idx = 0;
	while (window.poll()) {
		u32 image_idx;

		tie(result, image_idx) = device.device.acquireNextImageKHR(swapchain.swapchain, max_value<u32>, image_available_sem[frame_idx], {});
		ERROR_IF(failed(result), stl::fmt("Image acquire failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Image Acquired");

		result = device.device.waitForFences({ in_flight_fence[image_idx] }, true, max_value<u64>);
		ERROR_IF(failed(result), stl::fmt("Fence wait failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Fence Waited for");

		auto& cmd = command_buffers[image_idx];

		result = cmd.begin({
			.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		});
		ERROR_IF(failed(result), stl::fmt("Cmd Buffer begin failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Start Cmd Buffer");

		vk::ClearValue clear_val(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
		// Record Commands

		cmd.beginRenderPass({
			.renderPass = renderpass,
			.framebuffer = framebuffers[image_idx],
			.renderArea = scissor,
			.clearValueCount = 1,
			.pClearValues  = &clear_val,
		}, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		cmd.bindVertexBuffers(0, { buffer.first }, { 0 });
		cmd.draw(4, 1, 0, 0);

		cmd.endRenderPass();

		result = cmd.end();
		ERROR_IF(failed(result), stl::fmt("Cmd Buffer end failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("End Cmd Buffer");

		vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		result = device.device.resetFences({ in_flight_fence[image_idx] });
		ERROR_IF(failed(result), stl::fmt("Fence reset failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Fence Reset");

		vk::SubmitInfo submit_info = {
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &image_available_sem[frame_idx],
			.pWaitDstStageMask = &wait_stage,
			.commandBufferCount = 1,
			.pCommandBuffers = &cmd,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &render_finished_sem[frame_idx],
		};
		result = device.queues.graphics.submit({ submit_info }, in_flight_fence[image_idx]);
		ERROR_IF(failed(result), stl::fmt("Submission failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Submit");

		result = device.queues.present.presentKHR({
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &render_finished_sem[frame_idx],
			.swapchainCount = 1,
			.pSwapchains = &swapchain.swapchain,
			.pImageIndices = &image_idx,
		});
		ERROR_IF(failed(result), stl::fmt("Present failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Present");

		frame_idx = (frame_idx + 1) % swapchain.image_count;

		/*
		if (result == VK_ERROR_OUT_OF_DATE_KHR || windowResized)
		{
			windowResized = false;
			recreateSwapchain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw runtime_error("Image acquiring failed with " + to_string(result));
		}

		currentFrame = (currentFrame + 1) % maxFrameInFlight;*/
	}

	device.device.waitIdle();

	for (u32 i = 0; i < swapchain.image_count; ++i) {
		device.device.destroySemaphore(image_available_sem[i]);
		device.device.destroySemaphore(render_finished_sem[i]);
		device.device.destroyFence(in_flight_fence[i]);
	}

	device.allocator.destroyBuffer(buffer.first, buffer.second);

	device.device.destroyCommandPool(command_pool);
	device.device.destroyPipeline(pipeline);
	for (auto& framebuffer : framebuffers) {
		device.device.destroyFramebuffer(framebuffer);
	}
	device.device.destroyRenderPass(renderpass);
	device.device.destroyPipelineLayout(layout);

	for (auto& shader : shaders) {
		device.device.destroyShaderModule(shader);
	}

	swapchain.destroy();
	device.destroy();
	window.destroy();
	context.destroy();
	glfw.destroy();

	return 0;
}
