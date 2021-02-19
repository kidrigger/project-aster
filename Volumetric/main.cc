/*=========================================*/
/*  Volumetric: main.cc                    */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include <global.h>
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

#include <util/files.h>

#include <vector>

#include <renderdoc/renderdoc.h>

struct AtmosphereInfo {
	//ray
	alignas(16) vec3 scatter_coeff_rayleigh; //12
	alignas(04) f32 density_factor_rayleigh; //16

	// ozone
	alignas(16) vec3 absorption_coeff_ozone; //28
	alignas(04) f32 ozone_height;            //32
	alignas(04) f32 ozone_width;             //36

	//mei
	alignas(04) f32 scatter_coeff_mei;    //40
	alignas(04) f32 absorption_coeff_mei; //44
	alignas(04) f32 density_factor_mei;   //48

	alignas(04) f32 asymmetry_mei; //52

	// sampling
	alignas(04) i32 depth_samples; //56
	alignas(04) i32 view_samples;  //60
};

struct SunData {
	alignas(16) vec3 direction;
	alignas(04) i32 _pad0;
	alignas(16) vec3 intensities;
	alignas(04) i32 _pad1;
};

struct TransmittanceContext {
	Pipeline* pipeline{};
	RenderPass renderpass;
	vk::Framebuffer framebuffer;

	Image lut;
	ImageView lut_view;
	vk::Sampler lut_sampler;

	PipelineFactory* parent_factory;

	TransmittanceContext(PipelineFactory* _pipeline_factory, const AtmosphereInfo& _atmos);
	void recalculate(PipelineFactory* _pipeline_factory, const AtmosphereInfo& _atmos);
	~TransmittanceContext();
};

struct SkyViewContext {
	Pipeline* pipeline;
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

	SkyViewContext(PipelineFactory* _pipeline_factory, TransmittanceContext* _transmittance);
	void update(Camera* _camera, SunData* _sun_data, AtmosphereInfo* _atmos);
	void recalculate(vk::CommandBuffer _cmd);
	~SkyViewContext();
};

i32 main() {

	g_logger.set_minimum_logging_level(Logger::LogType::eDebug);

	Camera camera;
	CameraController camera_controller;

	GlfwContext glfw{};
	Context context{ "Aster Core", { 0, 0, 1 } };
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
			score += 5;
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
	camera.init({ 0, 1000, 0 }, { 0, 0, 1 }, window.extent, 0.1f, 30.0f, 70_deg);
	camera_controller.init(&window, &camera, 10000.0f);
	PipelineFactory pipeline_factory{ &device };

	Gui::Init(&swapchain);

	rdoc::init();

	vk::Result result;
	Pipeline* pipeline;
	RenderPass render_pass;
	std::vector<vk::Framebuffer> framebuffers;

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
	tie(result, render_pass) = RenderPass::create("Triangle Draw Pass", &device, {
		.attachmentCount = 1,
		.pAttachments = &attach_desc,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency,
	});
	ERROR_IF(failed(result), std::fmt("Renderpass creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("Renderpass Created");

	auto recreate_framebuffers = std::function{
		[&render_pass, &device, &framebuffers, &swapchain]() {
			vk::Result result_;
			for (auto& fb : framebuffers) {
				device.device.destroyFramebuffer(fb);
			}
			framebuffers.resize(swapchain.image_count);

			for (u32 i = 0; i < swapchain.image_count; ++i) {
				tie(result_, framebuffers[i]) = device.device.createFramebuffer({
					.renderPass = render_pass.renderpass,
					.attachmentCount = 1,
					.pAttachments = &swapchain.image_views[i],
					.width = swapchain.extent.width,
					.height = swapchain.extent.height,
					.layers = 1,
				});
				ERROR_IF(failed(result_), std::fmt("Framebuffer creation failed with %s", to_cstr(result_))) THEN_CRASH(result_) ELSE_INFO("Framebuffer Created");
			}
		}
	};

	recreate_framebuffers();

	tie(result, pipeline) = pipeline_factory.create_pipeline({
		.renderpass = render_pass,
		.viewport_state = {
			.enable_dynamic = true,
		},
		.shader_files = { R"(res/shaders/hillaire.vs.spv)", R"(res/shaders/hillaire.fs.spv)" },
		.dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor },
		.name = "Main Pipeline"
	});
	ERROR_IF(failed(result), std::fmt("Pipeline creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("Pipeline Created");

	std::vector<vk::DescriptorPoolSize> pool_sizes = {
		{
			.type = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = swapchain.image_count * 3,
		},
		{
			.type = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = swapchain.image_count
		},
		{
			.type = vk::DescriptorType::eSampledImage,
			.descriptorCount = swapchain.image_count
		}
	};

	vk::DescriptorPool descriptor_pool;
	tie(result, descriptor_pool) = device.device.createDescriptorPool({
		.maxSets = swapchain.image_count,
		.poolSizeCount = cast<u32>(pool_sizes.size()),
		.pPoolSizes = pool_sizes.data(),
	});

	std::vector<vk::DescriptorSet> descriptor_sets;
	{
		std::vector<vk::DescriptorSetLayout> layouts(swapchain.image_count, pipeline->layout->descriptor_set_layouts.front());
		tie(result, descriptor_sets) = device.device.allocateDescriptorSets({
			.descriptorPool = descriptor_pool,
			.descriptorSetCount = cast<u32>(layouts.size()),
			.pSetLayouts = layouts.data(),
		});
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

		void destroy() {
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
	for (u32 i = 0; i < swapchain.image_count; ++i) {
		tie(result, uniform_buffers.emplace_back()) = Buffer::create(std::fmt("Camera Ubo %i", i), &device, closest_multiple(sizeof(Camera), 256) + closest_multiple(sizeof(SunData), 256) + closest_multiple(sizeof(AtmosphereInfo), 256), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);
		{
			std::vector<u8> buf_data(closest_multiple(sizeof(Camera), 256) + closest_multiple(sizeof(SunData), 256) + closest_multiple(sizeof(AtmosphereInfo), 256));
			usize cam_offset, sun_offset, atmos_offset;
			usize offset = 0;
			cam_offset = offset;
			memcpy(buf_data.data(), &camera, sizeof(Camera));
			offset += closest_multiple(sizeof(Camera), 256);
			sun_offset = offset;
			memcpy(buf_data.data() + offset, &sun, sizeof(SunData)); // TODO Fix hard-code
			offset += closest_multiple(sizeof(SunData), 256);
			atmos_offset = offset;
			memcpy(buf_data.data() + offset, &atmosphere_info, sizeof(AtmosphereInfo)); // TODO Fix hard-code
			device.update_data(&uniform_buffers.back(), std::span<u8>(buf_data.data(), buf_data.size()));

			std::vector<vk::DescriptorBufferInfo> buf_info = {
				{
					.buffer = uniform_buffers.back().buffer,
					.offset = cam_offset,
					.range = sizeof(Camera),
				},
				{
					.buffer = uniform_buffers.back().buffer,
					.offset = sun_offset,
					.range = sizeof(SunData),
				},
				{
					.buffer = uniform_buffers.back().buffer,
					.offset = atmos_offset,
					.range = sizeof(AtmosphereInfo),
				}
			};

			vk::DescriptorImageInfo transmittance_image_info = {
				.sampler = transmittance.lut_sampler,
				.imageView = transmittance.lut_view.image_view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			};

			vk::DescriptorImageInfo sky_view_image_info = {
				.imageView = sky_view.lut_view.image_view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			};

			vk::WriteDescriptorSet wd0 = {
				.dstSet = descriptor_sets[i],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &buf_info[0],
			};

			vk::WriteDescriptorSet wd1 = {
				.dstSet = descriptor_sets[i],
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &buf_info[1],
			};

			vk::WriteDescriptorSet wd2 = {
				.dstSet = descriptor_sets[i],
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &buf_info[2],
			};

			vk::WriteDescriptorSet wd3 = {
				.dstSet = descriptor_sets[i],
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &transmittance_image_info,
			};

			vk::WriteDescriptorSet wd4 = {
				.dstSet = descriptor_sets[i],
				.dstBinding = 4,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = &sky_view_image_info,
			};

			device.device.updateDescriptorSets({ wd0, wd1, wd2, wd3, wd4 }, {});
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
			result = device.device.waitForFences({ current_frame->in_flight_fence }, true, max_value<u64>);
		}

		{
			OPTICK_EVENT("Acquire");

			tie(result, image_idx) = device.device.acquireNextImageKHR(swapchain.swapchain, max_value<u32>, current_frame->image_available_sem, {});

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
			result = device.device.waitForFences({ in_flight_frames[image_idx]->in_flight_fence }, true, max_value<u64>);
			ERROR_IF(failed(result), std::fmt("Fence wait failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Fence Waited for");
			in_flight_frames[image_idx] = current_frame;
		}

		{
			OPTICK_EVENT("Ubo Update");
			camera_controller.update();
			camera.update();

			std::vector<u8> buf_data(uniform_buffers[frame_idx].size);
			memcpy(buf_data.data(), &camera, sizeof(Camera));
			memcpy(buf_data.data() + closest_multiple(sizeof(Camera), 256), &sun, sizeof(SunData));                                                             // TODO Fix hardcode
			memcpy(buf_data.data() + closest_multiple(sizeof(Camera), 256) + closest_multiple(sizeof(SunData), 256), &atmosphere_info, sizeof(AtmosphereInfo)); // TODO Fix hardcode
			device.update_data(&uniform_buffers[frame_idx], std::span<u8>(buf_data.data(), buf_data.size()));
		}

		sky_view.update(&camera, &sun, &atmosphere_info);

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
			.framebuffer = framebuffers[image_idx],
			.renderArea = {
				.offset = { 0, 0 },
				.extent = swapchain.extent,
			},
			.clearValueCount = 1,
			.pClearValues = &clear_val,
		}, vk::SubpassContents::eInline);

		cmd.setViewport(0, {
			{
				.x = 0,
				.y = cast<f32>(swapchain.extent.height),
				.width = cast<f32>(swapchain.extent.width),
				.height = -cast<f32>(swapchain.extent.height),
				.minDepth = 0.0f,
				.maxDepth = 1.0f,
			} });
		cmd.setScissor(0, {
			{
				.offset = { 0, 0 },
				.extent = swapchain.extent,
			} });

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->pipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->layout->layout, 0, { descriptor_sets[frame_idx] }, {});
		cmd.draw(4, 1, 0, 0);

		cmd.endRenderPass();

		cmd.endDebugUtilsLabelEXT();

		Gui::Draw(cmd, image_idx);

		result = cmd.end();
		ERROR_IF(failed(result), std::fmt("Cmd Buffer end failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("End Cmd Buffer");

		vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		result = device.device.resetFences({ current_frame->in_flight_fence });
		ERROR_IF(failed(result), std::fmt("Fence reset failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Fence Reset");

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

			ERROR_IF(failed(result), std::fmt("Submission failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_VERBOSE("Submit");
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

	device.device.destroyDescriptorPool(descriptor_pool);

	pipeline->destroy();
	for (auto& framebuffer_ : framebuffers) {
		device.device.destroyFramebuffer(framebuffer_);
	}
	render_pass.destroy();

	Gui::Destroy();
	camera_controller.destroy();
	camera.destroy();

	return 0;
}

TransmittanceContext::TransmittanceContext(PipelineFactory* _pipeline_factory, const AtmosphereInfo& _atmos) {
	vk::Result result;

	parent_factory = _pipeline_factory;
	auto* device = _pipeline_factory->parent_device;

	tie(result, lut) = Image::create("Transmittance LUT", device, vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat, { 64, 256, 1 }, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
	ERROR_IF(failed(result), std::fmt("LUT Image could not be created with %s", to_cstr(result)));

	tie(result, lut_view) = ImageView::create(&lut, vk::ImageViewType::e2D, {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.levelCount = 1,
		.layerCount = 1,
	});
	ERROR_IF(failed(result), "LUT Image View could not be created");

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
		.framebuffer = framebuffer,
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
	auto* device = parent_factory->parent_device;

	lut.destroy();
	lut_view.destroy();
	device->device.destroySampler(lut_sampler);

	pipeline->destroy();
	device->device.destroyFramebuffer(framebuffer);
	renderpass.destroy();
}

SkyViewContext::SkyViewContext(PipelineFactory* _pipeline_factory, TransmittanceContext* _transmittance) {

	vk::Result result;
	parent_factory = _pipeline_factory;
	auto* const device = _pipeline_factory->parent_device;

	tie(result, ubo) = Buffer::create("Skyview uniform buffer", device, closest_multiple(sizeof(Camera), 256) + closest_multiple(sizeof(SunData), 256) + closest_multiple(sizeof(AtmosphereInfo), 256), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);
	ERROR_IF(failed(result), std::fmt("Skyview UBO creation failed with %s", to_cstr(result)));

	transmittance = _transmittance;

	tie(result, lut) = Image::create("Skyview LUT", device, vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat, { 192, 108, 1 }, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
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
		.name = "Skyview LUT Pipeline",
	});
	ERROR_IF(failed(result), std::fmt("Skyview LUT Pipeline creation failed with %s", to_cstr(result)));

	tie(result, framebuffer) = device->device.createFramebuffer({
		.renderPass = renderpass.renderpass,
		.attachmentCount = 1,
		.pAttachments = &lut_view.image_view,
		.width = lut.extent.width,
		.height = lut.extent.height,
		.layers = 1,
	});
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
		offset += closest_multiple(sizeof(Camera), 256);
		sun_offset = offset;
		offset += closest_multiple(sizeof(SunData), 256);
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
			.sampler = transmittance->lut_sampler,
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

void SkyViewContext::update(Camera* _camera, SunData* _sun_data, AtmosphereInfo* _atmos) {
	std::vector<u8> buf_data(ubo.size);
	memcpy(buf_data.data(), _camera, sizeof(Camera));
	memcpy(buf_data.data() + closest_multiple(sizeof(Camera), 256), _sun_data, sizeof(SunData));                                              // TODO Fix hardcode
	memcpy(buf_data.data() + closest_multiple(sizeof(Camera), 256) + closest_multiple(sizeof(SunData), 256), _atmos, sizeof(AtmosphereInfo)); // TODO Fix hardcode
	parent_factory->parent_device->update_data(&ubo, std::span<u8>(buf_data.data(), buf_data.size()));
}

void SkyViewContext::recalculate(vk::CommandBuffer _cmd) {

	OPTICK_EVENT("Recalculate Skyview");
	_cmd.beginDebugUtilsLabelEXT({
		.pLabelName = "Skyview LUT Calculation",
		.color = std::array{ 0.1f, 0.0f, 0.5f, 1.0f },
	});

	vk::ClearValue clear_val(std::array{ 0.0f, 1.0f, 0.0f, 1.0f });
	_cmd.beginRenderPass({
		.renderPass = renderpass.renderpass,
		.framebuffer = framebuffer,
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

	device->device.destroyFramebuffer(framebuffer);
	pipeline->destroy();
	renderpass.destroy();

	lut_view.destroy();
	lut.destroy();
}
