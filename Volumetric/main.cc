// =============================================
//  Volumetric: main.cc
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#include <stdafx.h>
#include <logger.h>
#include <set>

#include <core/glfw_context.h>
#include <core/window.h>
#include <core/context.h>
#include <core/device.h>
#include <core/pipeline.h>
#include <core/swapchain.h>
#include <core/camera.h>
#include <core/gui.h>
#include <core/image.h>
#include <core/image_view.h>
#include <core/resource_pool.h>

#include <util/buffer_writer.h>

#include <vector>

#include <renderdoc/renderdoc.h>
#include <optick/optick.h>

#include <sun_data.h>
#include <atmosphere_info.h>
#include <transmittance_context.h>
#include <sky_view_context.h>

i32 aster_main() {

	g_logger.set_minimum_logging_level(Logger::LogType::eDebug);
	GlfwContext glfw{};
	Context context{ "Volumetric Core", { 0, 0, 1 } };
	Window window{ PROJECT_NAME, &context, { 1280u, 720u } };

	vk::PhysicalDeviceFeatures enabled_device_features = {
		.depthClamp = true,
		.samplerAnisotropy = true,
		.shaderSampledImageArrayDynamicIndexing = true,
	};

#pragma region Device Selection
	using DInfo = Device::PhysicalDeviceInfo;
	auto physical_device_info = DeviceSelector{ &context, &window }.select_on([](DInfo& _inf) {
		return _inf.queue_family_indices.has_graphics() && _inf.queue_family_indices.has_present();
	}).select_on([_context = &context](DInfo& _inf) {
		auto [result, extension_properties] = _inf.device.enumerateDeviceExtensionProperties();
		std::vector<std::string> extensions(extension_properties.size());
		std::ranges::transform(extension_properties, extensions.begin(), [](vk::ExtensionProperties& _ext) {
			return cast<const char*>(_ext.extensionName);
		});
		const std::set<std::string> extension_set(extensions.begin(), extensions.end());
		return std::ranges::all_of(_context->device_extensions, [&extension_set](auto& _ext) {
			return extension_set.contains(_ext);
		});
	}).select_on([_window = &window](DInfo& _inf) {
		auto [result0, surface_formats] = _inf.device.getSurfaceFormatsKHR(_window->surface);
		if (failed(result0) || surface_formats.empty()) return false;

		auto [result1, present_modes] = _inf.device.getSurfacePresentModesKHR(_window->surface);
		if (failed(result1) || present_modes.empty()) return false;

		return true;
	}).sort_by([](DInfo& _inf) {
		u32 score = 0;
		if (_inf.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
			score += 2;
		} else if (_inf.properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
			score += 1;
		}

		if (_inf.queue_family_indices.has_compute()) {
			score++;
		}

		const auto& device_features = _inf.features;
		if (device_features.samplerAnisotropy) {
			score++;
		}
		if (device_features.shaderSampledImageArrayDynamicIndexing) {
			score++;
		}
		if (device_features.depthClamp) {
			score++;
		}
		return score;
	}).get();
#pragma endregion

	INFO(std::fmt("Using %s", physical_device_info.properties.deviceName.data()));

	Device device{ "Primary", &context, physical_device_info, enabled_device_features };
	Swapchain swapchain{ window.name, &window, &device };
	Camera camera{ { 0.0f, 1000.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, window.extent, 0.1f, 30.0f, 70_deg };
	CameraController camera_controller{ &window, &camera, 10000.0f };
	PipelineFactory pipeline_factory{ &device };

	Gui::Init(&swapchain);

	rdoc::init();

	vk::Result result;
	Pipeline* pipeline;
	RenderPass render_pass;
	std::vector<Framebuffer> framebuffers;

	vk::AttachmentDescription attach_desc = {
		.format = swapchain.format,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
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

	// Render pass
	render_pass = RenderPass::create("Triangle Draw Pass", &device, {
		.attachmentCount = 1,
		.pAttachments = &attach_desc,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency,
	}).expect(std::fmt("Renderpass creation failed with %s", to_cstr(err)));
	INFO(std::fmt("Renderpass %s Created", render_pass.name.c_str()));

	auto recreate_framebuffers = std::function{
		[&render_pass, &framebuffers, &swapchain]() {
			for (auto& fb : framebuffers) {
				fb.destroy();
			}
			framebuffers.resize(swapchain.image_count);

			for (u32 i = 0; i < swapchain.image_count; ++i) {
				framebuffers[i] = Framebuffer::create("Present Framebuffer", &render_pass, { swapchain.image_views[i] }, 1)
				                  .expect(std::fmt("Framebuffer creation failed with %s", to_cstr(err)));
			}
		}
	};

	recreate_framebuffers();

	pipeline = pipeline_factory.create_pipeline({
		.renderpass = render_pass,
		.viewport_state = {
			.enable_dynamic = true,
		},
		.shader_files = { R"(res/shaders/hillaire.vs.spv)", R"(res/shaders/hillaire.fs.spv)" },
		.dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor },
		.name = "Main Pipeline"
	}).expect(std::fmt("Pipeline creation failed with %s", to_cstr(err)));
	INFO("Pipeline Created");

	ResourcePool resource_pool = ResourcePool::create(&device, pipeline->layout, swapchain.image_count)
	                             .expect(std::fmt("Resource Binders creation failed with %s", to_cstr(err)));
	INFO(std::fmt("Resource Binders for pipeline %s successfully created", pipeline->name.c_str()));

	std::vector<ResourceSet> resource_sets;
	resource_sets.reserve(swapchain.image_count);
	for (u32 i = 0; i < swapchain.image_count; ++i) {
		auto res = resource_pool.allocate_resource_set();
		resource_sets.emplace_back(res.expect("Resource Set Alloc failed!"));
	}

	struct Frame {
		vk::Semaphore image_available_sem;
		vk::Semaphore render_finished_sem;
		vk::Fence in_flight_fence;

		vk::CommandPool command_pool;
		vk::CommandBuffer command_buffer;

		const Device* parent_device{};

		void init(Device* _device, u32 _frame_index) {
			parent_device = _device;
			vk::Result result;

			tie(result, image_available_sem) = _device->device.createSemaphore({});
			ERROR_IF(failed(result), std::fmt("Image available semaphore creation failed with %s", to_cstr(result))) THEN_CRASH(result);
			_device->set_object_name(image_available_sem, std::fmt("Frame %d Image Available Sem", _frame_index));

			tie(result, render_finished_sem) = _device->device.createSemaphore({});
			ERROR_IF(failed(result), std::fmt("Render finished semaphore creation failed with %s", to_cstr(result))) THEN_CRASH(result);
			_device->set_object_name(render_finished_sem, std::fmt("Frame %d Render Finished Sem", _frame_index));

			tie(result, in_flight_fence) = _device->device.createFence({ .flags = vk::FenceCreateFlagBits::eSignaled });
			ERROR_IF(failed(result), std::fmt("In flight fence creation failed with %s", to_cstr(result))) THEN_CRASH(result);
			_device->set_object_name(render_finished_sem, std::fmt("Frame %d In Flight Fence", _frame_index));

			tie(result, command_pool) = _device->device.createCommandPool({
				.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = _device->queue_families.graphics_idx,
			});
			ERROR_IF(failed(result), std::fmt("Command pool creation failed with %s", to_cstr(result))) THEN_CRASH(result);
			_device->set_object_name(command_pool, std::fmt("Frame %d Command Pool", _frame_index));

			vk::CommandBufferAllocateInfo cmd_buf_alloc_info = {
				.commandPool = command_pool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = 1,
			};
			result = _device->device.allocateCommandBuffers(&cmd_buf_alloc_info, &command_buffer);
			ERROR_IF(failed(result), std::fmt("Cmd Buffer allocation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Cmd Allocated Buffer");
			_device->set_object_name(command_buffer, std::fmt("Frame %d Command Buffer", _frame_index));
		}

		void destroy() const {
			parent_device->device.destroySemaphore(image_available_sem);
			parent_device->device.destroySemaphore(render_finished_sem);
			parent_device->device.destroyFence(in_flight_fence);
			parent_device->device.destroyCommandPool(command_pool);
		}
	};

	std::vector<Frame> frames(swapchain.image_count);
	u32 i_ = 0;
	for (auto& frame : frames) {
		frame.init(&device, i_++);
	}
	std::vector<Frame*> in_flight_frames;
	in_flight_frames.reserve(swapchain.image_count);
	for (auto& frame : frames) {
		in_flight_frames.push_back(&frame);
	}

	SunData sun = {
		.direction = normalize(vec3(0.0f, 0.0f, 1.0f)),
		.intensities = vec3(12.8f),
	};

	AtmosphereInfo atmosphere_info = {
		.scatter_coeff_rayleigh = vec3(5.802, 13.558, 33.1) * 1.0e-6f,
		.density_factor_rayleigh = 8000.0f,
		.absorption_coeff_ozone = vec3(0.650, 1.881, 0.085) * 1.0e-6f,
		.ozone_height = 25000.0f,
		.ozone_width = 30000.0f,
		.scatter_coeff_mei = 3.996f * 1.0e-6f,
		.absorption_coeff_mei = 4.40f * 1.0e-6f,
		.density_factor_mei = 1200.0f,
		.asymmetry_mei = 0.8f,
		.depth_samples = 3000,
		.view_samples = 3000,
	};

#pragma region ======== LUT Setup ==================================================================================================================

	TransmittanceContext transmittance{ &pipeline_factory, atmosphere_info };

	SkyViewContext sky_view{ &pipeline_factory, &transmittance };

#pragma endregion

	// ======== Buffer Setup ==================================================================================================================
	std::vector<Buffer> uniform_buffers;
	const auto ubo_alignment = device.physical_device_properties.limits.minUniformBufferOffsetAlignment;
	for (u32 i = 0; i < swapchain.image_count; ++i) {
		uniform_buffers.emplace_back() = Buffer::create(std::fmt("Camera Ubo %i", i), &device, closest_multiple(sizeof(Camera), ubo_alignment) + closest_multiple(sizeof(SunData), ubo_alignment) + closest_multiple(sizeof(AtmosphereInfo), ubo_alignment), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu)
		                                 .expect("Camera UBO creation failed!");
		{
			BufferWriter{ &uniform_buffers.back() } << camera << sun << atmosphere_info;

			auto buffer_ = uniform_buffers.back().buffer;
			resource_sets[i].set_buffer("camera", {
				.buffer = buffer_,
				.offset = 0,
				.range = sizeof(Camera)
			});
			resource_sets[i].set_buffer("sun", {
				.buffer = buffer_,
				.offset = closest_multiple(sizeof(Camera), ubo_alignment),
				.range = sizeof(SunData)
			});
			resource_sets[i].set_buffer("atmos", {
				.buffer = buffer_,
				.offset = closest_multiple(sizeof(Camera), ubo_alignment) + closest_multiple(sizeof(SunData), ubo_alignment),
				.range = sizeof(AtmosphereInfo)
			});
			resource_sets[i].set_texture("transmittance_lut", {
				.sampler = transmittance.lut_sampler.sampler,
				.imageView = transmittance.lut_view.image_view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			});
			resource_sets[i].set_texture("skyview_lut", {
				.imageView = sky_view.lut_view.image_view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			});
			resource_sets[i].update();
		}
	}

	AtmosphereInfo atmosphere_ui_view = {
		.scatter_coeff_rayleigh = atmosphere_info.scatter_coeff_rayleigh * 1.0e+6f,
		.density_factor_rayleigh = atmosphere_info.density_factor_rayleigh / 1000.0f,
		.absorption_coeff_ozone = atmosphere_info.absorption_coeff_ozone * 1.0e+6f,
		.ozone_height = atmosphere_info.ozone_height / 1000.0f,
		.ozone_width = atmosphere_info.ozone_width / 1000.0f,
		.scatter_coeff_mei = atmosphere_info.scatter_coeff_mei * 1.0e+6f,
		.absorption_coeff_mei = atmosphere_info.absorption_coeff_mei * 1.0e+6f,
		.density_factor_mei = atmosphere_info.density_factor_mei / 1000.0f,
		.asymmetry_mei = atmosphere_info.asymmetry_mei,
		.depth_samples = atmosphere_info.depth_samples,
		.view_samples = atmosphere_info.view_samples,
	};

	u32 frame_idx = 0;

	f32 time_of_day = 0;
	b8 dynamic_time_of_day = false;
	f32 hrs_per_second = 0.4f;
	g_time.init();

	while (window.poll()) {

		OPTICK_FRAME("Main frame");
		u32 image_idx;

		g_time.update();
		Frame* current_frame = &frames[frame_idx];

		{
			OPTICK_EVENT("Frame wait");
			result = device.device.waitForFences({ current_frame->in_flight_fence }, true, MAX_VALUE<u64>);
		}

		{
			OPTICK_EVENT("Acquire");

			tie(result, image_idx) = device.device.acquireNextImageKHR(swapchain.swapchain, MAX_VALUE<u32>, current_frame->image_available_sem, {});

			INFO_IF(result == vk::Result::eSuboptimalKHR, std::fmt("Swapchain %s suboptimal", swapchain.name.data()))
			ELSE_IF_INFO(result == vk::Result::eErrorOutOfDateKHR, "Recreating Swapchain " + swapchain.name) DO(swapchain.recreate()) DO(recreate_framebuffers()) DO(Gui::Recreate())
			ELSE_IF_ERROR(failed(result), std::fmt("Image acquire failed with %s", to_cstr(result))) THEN_CRASH(result)
			ELSE_VERBOSE("Image Acquired");
		}

# pragma region ======== GUI ==================================================================================================================

		{
			OPTICK_EVENT("Gui build");
			Gui::StartBuild();

			Gui::Begin("Settings");

			Gui::PushItemWidth(-100);

			Gui::InputFloat("Altitude", &camera.position.y, 1000.0f, 10000.0f, "%.3f m");

			Gui::Checkbox("Dynamic Time of day", &dynamic_time_of_day);
			Gui::SliderFloat("Time ratio", &hrs_per_second, (1.0f / 60.0f), 3.0f, "%.3f min/sec");
			if (!dynamic_time_of_day) {
				Gui::DragFloat("Time of day##Input", &time_of_day, 0.01f, 0.0f, 24.0f, "%.2f");
			} else {
				time_of_day += hrs_per_second * cast<f32>(g_time.delta) / 0.6f;
				time_of_day = time_of_day >= 24.0f ? time_of_day - 24.0f : time_of_day;
				Gui::Text("[ %.2f ] Time of day", time_of_day);
			}
			f32 angle_ = 15.0_deg * time_of_day;
			sun.direction = vec3(0.0f, cos(angle_), -sin(angle_));

			Gui::DragFloat3("Sun Intensity", &sun.intensities[0], 0.1f, 0.0f, 128.0f);

			if (Gui::CollapsingHeader("Atmosphere")) {
				if (Gui::DragFloat("Rayleigh Density Factor", &atmosphere_ui_view.density_factor_rayleigh, 1.f, 0.0f, 100.0f, "%5.3f km")) {
					atmosphere_info.density_factor_rayleigh = atmosphere_ui_view.density_factor_rayleigh * 1000.0f;
				}
				if (Gui::DragFloat("Mei Density Factor", &atmosphere_ui_view.density_factor_mei, 1.f, 0.0f, 100.0f, "%5.3f km")) {
					atmosphere_info.density_factor_mei = atmosphere_ui_view.density_factor_mei * 1000.0f;
				}
				if (Gui::DragFloat("Ozone Height", &atmosphere_ui_view.ozone_height, 1.f, 0.0f, 100.0f, "%5.3f km")) {
					atmosphere_info.ozone_height = atmosphere_ui_view.ozone_height * 1000.0f;
				}
				if (Gui::DragFloat("Ozone Width", &atmosphere_ui_view.ozone_width, 1.f, 0.0f, 100.0f, "%5.3f km")) {
					atmosphere_info.ozone_width = atmosphere_ui_view.ozone_width * 1000.0f;
				}
				if (Gui::InputFloat3("Rayleigh Scatter", &atmosphere_ui_view.scatter_coeff_rayleigh[0], "%.4f e-6")) {
					atmosphere_info.scatter_coeff_rayleigh = atmosphere_ui_view.scatter_coeff_rayleigh * 1.0e-6f;
				}
				if (Gui::InputFloat("Mei Scatter", &atmosphere_ui_view.scatter_coeff_mei, 0.01f, 0.1f, "%.4f e-6")) {
					atmosphere_info.scatter_coeff_rayleigh = atmosphere_ui_view.scatter_coeff_rayleigh * 1.0e-6f;
				}
				Gui::InputInt("Depth Samples", &atmosphere_info.depth_samples, 10, 100);
				Gui::InputInt("View Samples", &atmosphere_info.view_samples, 1, 10);
				if (Gui::Button("Recalculate Transmittance")) {
					transmittance.recalculate(&pipeline_factory, atmosphere_info);
				}
			}

			Gui::End();

			Gui::EndBuild();
		}

#pragma endregion

		// ======== Updates ==================================================================================================================

		{
			OPTICK_EVENT("Image wait");
			result = device.device.waitForFences({ in_flight_frames[image_idx]->in_flight_fence }, true, MAX_VALUE<u64>);
			ERROR_IF(failed(result), std::fmt("Fence wait failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Fence Waited for");
			in_flight_frames[image_idx] = current_frame;
		}

		{
			OPTICK_EVENT("Ubo Update");
			camera_controller.update();
			camera.update();

			std::vector<u8> buf_data(uniform_buffers[frame_idx].size);
			memcpy(buf_data.data(), &camera, sizeof(Camera));
			memcpy(buf_data.data() + closest_multiple(sizeof(Camera), ubo_alignment), &sun, sizeof(SunData));                                                                       // TODO Fix hardcode
			memcpy(buf_data.data() + closest_multiple(sizeof(Camera), ubo_alignment) + closest_multiple(sizeof(SunData), ubo_alignment), &atmosphere_info, sizeof(AtmosphereInfo)); // TODO Fix hardcode
			device.update_data(&uniform_buffers[frame_idx], std::span<u8>(buf_data.data(), buf_data.size()));
		}

		sky_view.update(camera, sun, atmosphere_info);

		// ======== Record Commands ==================================================================================================================

		{
			OPTICK_EVENT("Reset Command Pool");
			device.device.resetCommandPool(current_frame->command_pool, {});
		}
		auto& cmd = current_frame->command_buffer;

		result = cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, });
		ERROR_IF(failed(result), std::fmt("Cmd Buffer begin failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Start Cmd Buffer");

		sky_view.recalculate(cmd);

		vk::ClearValue clear_val(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });

		cmd.beginDebugUtilsLabelEXT({
			.pLabelName = "Triangle Draw",
			.color = std::array{ 0.0f, 0.5f, 0.0f, 1.0f },
		});

		cmd.beginRenderPass({
			.renderPass = render_pass.renderpass,
			.framebuffer = framebuffers[image_idx].framebuffer,
			.renderArea = {
				.extent = swapchain.extent,
			},
			.clearValueCount = 1,
			.pClearValues = &clear_val,
		}, vk::SubpassContents::eInline);

		cmd.setViewport(0, {
			{
				.y = cast<f32>(swapchain.extent.height),
				.width = cast<f32>(swapchain.extent.width),
				.height = -cast<f32>(swapchain.extent.height),
				.maxDepth = 1.0f,
			} });
		cmd.setScissor(0, {
			{
				.offset = { 0, 0 },
				.extent = swapchain.extent,
			} });

		cmd.bindPipeline(pipeline->bind_point, pipeline->pipeline);
		cmd.bindDescriptorSets(pipeline->bind_point, pipeline->layout->layout, 0, resource_sets[frame_idx].sets, {});
		cmd.draw(4, 1, 0, 0);

		cmd.endRenderPass();

		cmd.endDebugUtilsLabelEXT();

		Gui::Draw(cmd, image_idx);

		result = cmd.end();
		ERROR_IF(failed(result), std::fmt("Cmd Buffer end failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("End Cmd Buffer");

		vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		result = device.device.resetFences({ current_frame->in_flight_fence });
		ERROR_IF(failed(result), std::fmt("Fence reset failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Fence Reset");

		// ======== Submit Commands ==================================================================================================================
		{
			OPTICK_EVENT("Submit");
			vk::SubmitInfo submit_info = vk::SubmitInfo{}
			                             .setWaitSemaphores(current_frame->image_available_sem)
			                             .setWaitDstStageMask(wait_stage)
			                             .setCommandBuffers(cmd)
			                             .setSignalSemaphores(current_frame->render_finished_sem);
			result = device.queues.graphics.submit({ submit_info }, current_frame->in_flight_fence);

			ERROR_IF(failed(result), std::fmt("Submission failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Submit");
		}


		// ======== Present ==================================================================================================================
		{
			OPTICK_EVENT("Present");
			result = device.queues.present.presentKHR(vk::PresentInfoKHR{}
			                                          .setWaitSemaphores(current_frame->render_finished_sem)
			                                          .setSwapchains(swapchain.swapchain)
			                                          .setImageIndices(image_idx)
			);
			INFO_IF(result == vk::Result::eSuboptimalKHR, std::fmt("Swapchain %s suboptimal", swapchain.name.data()))
			ELSE_IF_INFO(result == vk::Result::eErrorOutOfDateKHR, "Recreating Swapchain " + swapchain.name) DO(swapchain.recreate()) DO(recreate_framebuffers()) DO(Gui::Recreate())
			ELSE_IF_ERROR(failed(result), std::fmt("Present failed with %s", to_cstr(result))) THEN_CRASH(result)
			ELSE_VERBOSE("Present");
		}

		frame_idx = (frame_idx + 1) % swapchain.image_count;
	}

	result = device.device.waitIdle();
	ERROR_IF(failed(result), std::fmt("Idling failed with %s", to_cstr(result)));

	// ======== Cleanup ==================================================================================================================

	for (auto& frame : frames) {
		frame.destroy();
	}

	for (auto& buf : uniform_buffers) {
		buf.destroy();
	}

	resource_pool.destroy();

	pipeline->destroy();
	for (auto& framebuffer_ : framebuffers) {
		framebuffer_.destroy();
	}
	render_pass.destroy();

	Gui::Destroy();

	return 0;
}

i32 main() {
	try {
		aster_main();
	} catch (std::exception& e) {
		ERROR(e.what());
	}
}
