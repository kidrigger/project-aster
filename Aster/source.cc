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
#include <EASTL/fixed_vector.h>
#include <EASTL/vector_map.h>

struct ThreadContext {
	stl::fixed_vector<vk::CommandPool, 3> command_pools;
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
	vk::DescriptorSetLayout descriptor_layout;
	stl::fixed_vector<vk::Framebuffer, 3> framebuffers;

	vk::DescriptorSetLayoutBinding desc_set_layout_binding = {
		.binding = 0,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eVertex,
	};

	tie(result, descriptor_layout) = device.device.createDescriptorSetLayout({
		.bindingCount = 1,
		.pBindings = &desc_set_layout_binding,
	});

	tie(result, layout) = device.device.createPipelineLayout({
		.setLayoutCount = 1,
		.pSetLayouts = &descriptor_layout,
	});
	ERROR_IF(failed(result), stl::fmt("Layout creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Pipeline Layout Created");

	// Shaders
	auto vertex_code = load_binary32_file(R"(res/shaders/shader.vs.spv)");
	tie(result, shaders[0]) = device.device.createShaderModule(vk::ShaderModuleCreateInfo{
		.codeSize = sizeof(u32) * vertex_code.size(),
		.pCode = vertex_code.data()
	});
	ERROR_IF(failed(result), stl::fmt("Vertex shader creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Vertex Shader Created");
	device.set_object_name(shaders[0], R"(shader.vs)");

	auto pixel_code = load_binary32_file(R"(res/shaders/shader.fs.spv)");
	tie(result, shaders[1]) = device.device.createShaderModule(vk::ShaderModuleCreateInfo{
		.codeSize = sizeof(u32) * pixel_code.size(),
		.pCode = pixel_code.data()
	});
	ERROR_IF(failed(result), stl::fmt("Fragment shader creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Frahment Shader Created");
	device.set_object_name(shaders[1], R"(shader.fs)");

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
	device.set_object_name(renderpass, "Triangle Draw Pass");

	auto recreate_framebuffers = [&renderpass, &device, &framebuffers, &swapchain]() {
		vk::Result result;
		for (auto& fb : framebuffers) {
			device.device.destroyFramebuffer(fb);
		}
		framebuffers.resize(swapchain.image_count);

		for (u32 i = 0; i < swapchain.image_count; ++i) {
			tie(result, framebuffers[i]) = device.device.createFramebuffer({
				.renderPass = renderpass,
				.attachmentCount = 1,
				.pAttachments = &swapchain.image_views[i],
				.width = swapchain.extent.width,
				.height = swapchain.extent.height,
				.layers = 1,
				});
			ERROR_IF(failed(result), stl::fmt("Framebuffer creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Framebuffer Created");
		}
	};

	recreate_framebuffers();

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

	vk::PipelineViewportStateCreateInfo vsci = {
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
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

	stl::array<vk::DynamicState, 2> dynamic_states = { vk::DynamicState::eScissor, vk::DynamicState::eViewport };
	vk::PipelineDynamicStateCreateInfo dsci = {
		.dynamicStateCount = cast<u32>(dynamic_states.size()),
		.pDynamicStates = dynamic_states.data(),
	};

	tie(result, pipeline) = device.device.createGraphicsPipeline({/*Cache*/ }, {
		.stageCount = cast<u32>(ssci.size()),
		.pStages = ssci.data(),
		.pVertexInputState = &visci,
		.pInputAssemblyState = &iasci,
		.pViewportState = &vsci,
		.pRasterizationState = &rsci,
		.pMultisampleState = &msci,
		.pColorBlendState = &cbsci,
		.pDynamicState = &dsci,
		.layout = layout,
		.renderPass = renderpass,
	});
	ERROR_IF(failed(result), stl::fmt("Pipeline creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Pipeline Created");

	stl::vector<Vertex> vertices = {
		{ vec4(0.0f, 0.5f, 0.0f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ vec4(0.5f, -0.5f, 0.0f, 1.0f), vec4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ vec4(-0.5f, -0.5f, 0.0f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 1.0f) },
	};
	vertices.push_back(vertices.front());

	// Vertices
	Buffer buffer;
	tie(result, buffer) = device.create_buffer("Triangle VB", 4 * sizeof(Vertex), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vma::MemoryUsage::eGpuOnly);
	auto transfer_handle = device.upload_data(&buffer, stl::span((u8*)vertices.data(), sizeof(vertices[0]) * vertices.size()));

	vk::DescriptorPoolSize pool_size = {
		.type = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = swapchain.image_count,
	};

	vk::DescriptorPool descriptor_pool;
	tie(result, descriptor_pool) = device.device.createDescriptorPool({
		.maxSets = 3,
		.poolSizeCount = 1,
		.pPoolSizes = &pool_size,
	});

	stl::vector<vk::DescriptorSet> descriptor_sets;
	{
		stl::vector<vk::DescriptorSetLayout> layouts(swapchain.image_count, descriptor_layout);
		std::vector<vk::DescriptorSet> dsets;
		tie(result, dsets) = device.device.allocateDescriptorSets({
			.descriptorPool = descriptor_pool,
			.descriptorSetCount = cast<u32>(layouts.size()),
			.pSetLayouts = layouts.data(),
		});
		for (auto& ds : dsets) {
			descriptor_sets.push_back(ds);
		}
	}

	struct Frame {
		vk::Semaphore image_available_sem;
		vk::Semaphore render_finished_sem;
		vk::Fence in_flight_fence;

		vk::CommandPool command_pool;
		vk::CommandBuffer command_buffer;

		const Device* parent_device;

		void init(Device* _device, u32 frame_index) {
			parent_device = _device;
			vk::Result result;

			tie(result, image_available_sem) = _device->device.createSemaphore({});
			ERROR_IF(failed(result), stl::fmt("Image available semaphore creation failed with %s", to_cstring(result))) THEN_CRASH(result);
			_device->set_object_name(image_available_sem, stl::fmt("Frame %d Image Available Sem", frame_index));

			tie(result, render_finished_sem) = _device->device.createSemaphore({});
			ERROR_IF(failed(result), stl::fmt("Render finished semaphore creation failed with %s", to_cstring(result))) THEN_CRASH(result);
			_device->set_object_name(render_finished_sem, stl::fmt("Frame %d Render Finished Sem", frame_index));

			tie(result, in_flight_fence) = _device->device.createFence({ .flags = vk::FenceCreateFlagBits::eSignaled });
			ERROR_IF(failed(result), stl::fmt("In flight fence creation failed with %s", to_cstring(result))) THEN_CRASH(result);
			_device->set_object_name(render_finished_sem, stl::fmt("Frame %d In Flight Fence", frame_index));

			tie(result, command_pool) = _device->device.createCommandPool({
				.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = _device->queue_families.graphics_idx,
			});
			ERROR_IF(failed(result), stl::fmt("Command pool creation failed with %s", to_cstring(result))) THEN_CRASH(result);
			_device->set_object_name(command_pool, stl::fmt("Frame %d Command Pool", frame_index));

			vk::CommandBufferAllocateInfo cbai = {
				.commandPool = command_pool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = 1,
			};
			result = _device->device.allocateCommandBuffers(&cbai, &command_buffer);
			ERROR_IF(failed(result), stl::fmt("Cmd Buffer allocation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Cmd Allocated Buffer");
		}

		void destroy() {
			parent_device->device.destroySemaphore(image_available_sem);
			parent_device->device.destroySemaphore(render_finished_sem);
			parent_device->device.destroyFence(in_flight_fence);
			parent_device->device.destroyCommandPool(command_pool);
		}
	};

	stl::fixed_vector<Frame, 3> frames(swapchain.image_count);
	u32 i_ = 0;
	for (auto& frame : frames) {
		frame.init(&device, i_++);
	}
	stl::fixed_vector<Frame*, 3> in_flight_frames;
	in_flight_frames.reserve(swapchain.image_count);
	for (auto& frame : frames) {
		in_flight_frames.push_back(&frame);
	}

	stl::fixed_vector<Buffer, 3> ubos;
	for (u32 i = 0; i < swapchain.image_count; ++i) {
		tie(result, ubos.push_back()) = device.create_buffer(stl::fmt("Ubo %i", i), 4 * sizeof(Vertex), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);
		{
			void* mapped_memory;
			tie(result, mapped_memory) = device.allocator.mapMemory(ubos.back().allocation);
			ERROR_IF(failed(result), stl::fmt("Memory mapping failed with %s", to_cstring(result))) THEN_CRASH(result);
			auto model = cast<mat4*>(mapped_memory);
			*model = glm::mat4(1.0f);
			device.allocator.unmapMemory(ubos.back().allocation);

			vk::DescriptorBufferInfo buf_info = {
				.buffer = ubos.back().buffer,
				.offset = 0,
				.range = sizeof(mat4),
			};

			vk::WriteDescriptorSet wds = {
				.dstSet = descriptor_sets[i],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &buf_info,
			};

			device.device.updateDescriptorSets({ wds }, {});
		};
	}

	u32 frame_idx = 0;
	f64 ftime = 0;

	transfer_handle.wait_for_transfer();
	while (window.poll()) {

		OPTICK_FRAME("Main frame");
		u32 image_idx;

		ftime = glfwGetTime();

		Frame* current_frame = &frames[frame_idx];

		{
			OPTICK_EVENT("Frame wait");
			result = device.device.waitForFences({ current_frame->in_flight_fence }, true, max_value<u64>);
		}

		{
			OPTICK_EVENT("Acquire");

			tie(result, image_idx) = device.device.acquireNextImageKHR(swapchain.swapchain, max_value<u32>, current_frame->image_available_sem, {});

			INFO_IF(result == vk::Result::eSuboptimalKHR, stl::fmt("Swapchain %s suboptimal", swapchain.name.data()))
			ELSE_IF_INFO(result == vk::Result::eErrorOutOfDateKHR, "Recreating Swapchain " + swapchain.name) DO(swapchain.recreate()) DO(recreate_framebuffers())
			ELSE_IF_ERROR(failed(result), stl::fmt("Image acquire failed with %s", to_cstring(result))) THEN_CRASH(result)
			ELSE_VERBOSE("Image Acquired");
		}

		{
			OPTICK_EVENT("Image wait");
			result = device.device.waitForFences({ in_flight_frames[image_idx]->in_flight_fence }, true, max_value<u64>);
			ERROR_IF(failed(result), stl::fmt("Fence wait failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Fence Waited for");
			in_flight_frames[image_idx] = current_frame;
		}

		{
			void* mapped_memory;
			tie(result, mapped_memory) = device.allocator.mapMemory(ubos[frame_idx].allocation);
			ERROR_IF(failed(result), stl::fmt("Memory mapping failed with %s", to_cstring(result))) THEN_CRASH(result);
			auto model = cast<mat4*>(mapped_memory);
			*model = glm::transpose(glm::rotate(glm::mat4(1.0f), cast<f32>(ftime), glm::vec3(0.0f, 1.0f, 0.0f)));
			device.allocator.unmapMemory(ubos[frame_idx].allocation);
		}

		{
			OPTICK_EVENT("Reset Command Pool");
			device.device.resetCommandPool(current_frame->command_pool, {});
		}
		vk::CommandBuffer cmd = current_frame->command_buffer;

		result = cmd.begin({
			.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		});
		ERROR_IF(failed(result), stl::fmt("Cmd Buffer begin failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Start Cmd Buffer");

		vk::ClearValue clear_val(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
		// Record Commands

		cmd.setViewport(0, {{
			.x = 0,
			.y = 0,
			.width = cast<f32>(swapchain.extent.width),
			.height = cast<f32>(swapchain.extent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		}});
		cmd.setScissor(0, { {
			.offset = {0, 0},
			.extent = swapchain.extent,
		} });

		cmd.beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT{
			.pLabelName = "Triangle Draw",
			.color = std::array{0.0f, 1.0f, 0.0f, 0.0f},
		});

		cmd.beginRenderPass({
			.renderPass = renderpass,
			.framebuffer = framebuffers[image_idx],
			.renderArea = {
				.offset = {0, 0},
				.extent = swapchain.extent,
			},
			.clearValueCount = 1,
			.pClearValues  = &clear_val,
		}, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, { descriptor_sets[frame_idx] }, {});
		cmd.bindVertexBuffers(0, { buffer.buffer }, { 0 });
		cmd.draw(4, 1, 0, 0);

		cmd.endRenderPass();

		cmd.endDebugUtilsLabelEXT();

		result = cmd.end();
		ERROR_IF(failed(result), stl::fmt("Cmd Buffer end failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("End Cmd Buffer");

		vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		result = device.device.resetFences({ current_frame->in_flight_fence });
		ERROR_IF(failed(result), stl::fmt("Fence reset failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Fence Reset");

		{
			OPTICK_EVENT("Submit");
			vk::SubmitInfo submit_info = {
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &current_frame->image_available_sem,
				.pWaitDstStageMask = &wait_stage,
				.commandBufferCount = 1,
				.pCommandBuffers = &cmd,
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &current_frame->render_finished_sem,
			};
			result = device.queues.graphics.submit({ submit_info }, current_frame->in_flight_fence);
			ERROR_IF(failed(result), stl::fmt("Submission failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Submit");
		}

		{
			OPTICK_EVENT("Present");
			result = device.queues.present.presentKHR({
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &current_frame->render_finished_sem,
				.swapchainCount = 1,
				.pSwapchains = &swapchain.swapchain,
				.pImageIndices = &image_idx,
			});
			INFO_IF(result == vk::Result::eSuboptimalKHR, stl::fmt("Swapchain %s suboptimal", swapchain.name.data()))
			ELSE_IF_INFO(result == vk::Result::eErrorOutOfDateKHR, "Recreating Swapchain " + swapchain.name) DO(swapchain.recreate()) DO(recreate_framebuffers())
			ELSE_IF_ERROR(failed(result), stl::fmt("Present failed with %s", to_cstring(result))) THEN_CRASH(result)
			ELSE_VERBOSE("Present");
		}

		frame_idx = (frame_idx + 1) % swapchain.image_count;
	}

	result = device.device.waitIdle();
	ERROR_IF(failed(result), stl::fmt("Idling failed with %s", to_cstring(result)));

	for (auto& frame : frames) {
		frame.destroy();
	}

	for (auto& buf : ubos) {
		device.allocator.destroyBuffer(buf.buffer, buf.allocation);
	}

	device.device.destroyDescriptorPool(descriptor_pool);

	device.allocator.destroyBuffer(buffer.buffer, buffer.allocation);

	device.device.destroyPipeline(pipeline);
	for (auto& framebuffer : framebuffers) {
		device.device.destroyFramebuffer(framebuffer);
	}
	device.device.destroyRenderPass(renderpass);
	device.device.destroyDescriptorSetLayout(descriptor_layout);
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
