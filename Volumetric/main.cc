/*=========================================*/
/*  Volumetric: main.cc                    */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include <global.h>
#include <logger.h>
#include <cstdio>

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
#include <map>

#include <renderdoc/renderdoc.h>

struct AtmosphereInfo {
	//ray
	alignas(16) vec3 scatter_coeff_rayleigh;	//12
	alignas(04) f32 density_factor_rayleigh;	//16

	// ozone
	alignas(16) vec3 absorption_coeff_ozone;	//28
	alignas(04) f32 ozone_height;				//32
	alignas(04) f32 ozone_width;				//36

	//mei
	alignas(04) f32 scatter_coeff_mei;			//40
	alignas(04) f32 absorption_coeff_mei;		//44
	alignas(04) f32 density_factor_mei;			//48

	alignas(04) f32 assymetry_mei;				//52

	// sampling
	alignas(04) i32 depth_samples;				//56
	alignas(04) i32 view_samples;				//60
};

inline static Pipeline* lut_pipeline = nullptr;

void calculate_transmittance_lut(Device* _device, PipelineFactory* _pipeline_factory, const AtmosphereInfo& _atmos, Image* _image);

int main() {

	g_logger.set_minimum_logging_level(Logger::LogType::eDebug);

	GLFWContext glfw;
	Context context;
	Window window;
	Device device;
	Swapchain swapchain;
	Camera camera;
	CameraController camera_controller;
	PipelineFactory pipeline_factory;

	glfw.init();
	context.init("Aster Core", Version{ 0, 0, 1 });
	window.init(&context, 1280u, 720u, PROJECT_NAME, false);
	device.init("Primary", &context, &window);
	swapchain.init(window.name, &window, &device);
	camera.init(vec3(0, 1, 0), vec3(0, 0, 1), window.extent, 0.1f, 30.0f, 70_deg);
	camera_controller.init(&window, &camera, 10000.0f);
	pipeline_factory.init(&device);

	Gui::Init(&swapchain);

	rdoc::init();

	vk::Result result;
	Pipeline* pipeline;
	RenderPass renderpass;
	stl::vector<vk::Framebuffer> framebuffers;

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

	// Renderpass
	tie(result, renderpass) = pipeline_factory.create_renderpass("Triangle Draw Pass", {
		.attachmentCount = 1,
		.pAttachments = &attach_desc,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency,
	});
	ERROR_IF(failed(result), stl::fmt("Renderpass creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Renderpass Created");

	auto recreate_framebuffers_impl = [&renderpass, &device, &framebuffers, &swapchain]() {
		vk::Result result;
		for (auto& fb : framebuffers) {
			device.device.destroyFramebuffer(fb);
		}
		framebuffers.resize(swapchain.image_count);

		for (u32 i = 0; i < swapchain.image_count; ++i) {
			tie(result, framebuffers[i]) = device.device.createFramebuffer({
				.renderPass = renderpass.renderpass,
				.attachmentCount = 1,
				.pAttachments = &swapchain.image_views[i],
				.width = swapchain.extent.width,
				.height = swapchain.extent.height,
				.layers = 1,
			});
			ERROR_IF(failed(result), stl::fmt("Framebuffer creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Framebuffer Created");
		}
	};
	auto recreate_framebuffers = [&recreate_framebuffers_impl]() {
		return recreate_framebuffers_impl();
	};

	recreate_framebuffers();

	tie(result, pipeline) = pipeline_factory.create_pipeline({
		.renderpass = renderpass,
		.viewport_state = {
			.enable_dynamic = true,
		},
		.shader_files = { R"(res/shaders/hillaire.vs.spv)", R"(res/shaders/hillaire.fs.spv)" },
		.dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor },
		.name = "Main Pipeline"
	});
	ERROR_IF(failed(result), stl::fmt("Pipeline creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Pipeline Created");

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
		stl::vector<vk::DescriptorSetLayout> layouts(swapchain.image_count, pipeline->layout->descriptor_set_layouts.front());
		std::vector<vk::DescriptorSet> descriptor_sets;
		tie(result, descriptor_sets) = device.device.allocateDescriptorSets({
			.descriptorPool = descriptor_pool,
			.descriptorSetCount = cast<u32>(layouts.size()),
			.pSetLayouts = layouts.data(),
		});
		for (auto& ds : descriptor_sets) {
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
			_device->set_object_name(command_buffer, stl::fmt("Frame %d Command Buffer", frame_index));
		}

		void destroy() {
			parent_device->device.destroySemaphore(image_available_sem);
			parent_device->device.destroySemaphore(render_finished_sem);
			parent_device->device.destroyFence(in_flight_fence);
			parent_device->device.destroyCommandPool(command_pool);
		}
	};

	stl::vector<Frame> frames(swapchain.image_count);
	u32 i_ = 0;
	for (auto& frame : frames) {
		frame.init(&device, i_++);
	}
	stl::vector<Frame*> in_flight_frames;
	in_flight_frames.reserve(swapchain.image_count);
	for (auto& frame : frames) {
		in_flight_frames.push_back(&frame);
	}

	struct SunData {
		alignas(16) vec3 direction;
		alignas(04) i32 _pad0;
		alignas(16) vec3 intensities;
		alignas(04) i32 _pad1;
	};

	SunData sun = {
		.direction = glm::normalize(vec3(0.0f, 0.0f, 1.0f)),
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
		.assymetry_mei = 0.8f,
		.depth_samples = 3000,
		.view_samples = 50,
	};

	Image transmittance_lut;
	tie(result, transmittance_lut) = Image::create(&device, "Transmittance LUT", vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat, { 64, 256, 1 }, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
	ERROR_IF(failed(result), stl::fmt("LUT Image could not be created with %s", to_cstring(result)));

	calculate_transmittance_lut(&device, &pipeline_factory, atmosphere_info, &transmittance_lut);

	vk::ImageView transmittance_view;
	vk::Sampler transmittance_sampler;
	tie(result, transmittance_view) = device.device.createImageView({
		.image = transmittance_lut.image,
		.viewType = vk::ImageViewType::e2D,
		.format = transmittance_lut.format,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.levelCount = 1,
			.layerCount = 1,
		},
	});
	ERROR_IF(failed(result), "LUT Image View could not be created");
	device.set_object_name(transmittance_view, transmittance_lut.name + " view");
	tie(result, transmittance_sampler) = device.device.createSampler({
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.addressModeU = vk::SamplerAddressMode::eClampToEdge,
		.addressModeV = vk::SamplerAddressMode::eClampToEdge,
	});
	ERROR_IF(failed(result), "LUT Image Sampler could not be created");
	device.set_object_name(transmittance_view, transmittance_lut.name + " sampler");

	auto closest_multiple = [](usize _val, usize _of) -> usize {
		return _of * ((_val + _of - 1) / _of);
	};

	stl::vector<Buffer> ubos;
	for (u32 i = 0; i < swapchain.image_count; ++i) {
		tie(result, ubos.emplace_back()) = Buffer::create(&device, stl::fmt("Camera Ubo %i", i), closest_multiple(sizeof(Camera), 256) + closest_multiple(sizeof(SunData), 256) + closest_multiple(sizeof(AtmosphereInfo), 256), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eCpuToGpu);
		{
			stl::vector<u8> buf_data(closest_multiple(sizeof(Camera), 256) + closest_multiple(sizeof(SunData), 256) + closest_multiple(sizeof(AtmosphereInfo), 256));
			usize cam_offset, sun_offset, atmos_offset;
			usize offset = 0;
			cam_offset = offset;
			memcpy(buf_data.data(), &camera, sizeof(Camera));
			offset += closest_multiple(sizeof(Camera), 256);
			sun_offset = offset;
			memcpy(buf_data.data() + offset, &sun, sizeof(SunData));	// TODO Fix hardcode
			offset += closest_multiple(sizeof(SunData), 256);
			atmos_offset = offset;
			memcpy(buf_data.data() + offset, &atmosphere_info, sizeof(AtmosphereInfo));	// TODO Fix hardcode
			device.update_data(&ubos.back(), stl::span<u8>(buf_data.data(), buf_data.size()));

			stl::vector<vk::DescriptorBufferInfo> buf_info = {
				{
					.buffer = ubos.back().buffer,
					.offset = cam_offset,
					.range = sizeof(Camera),
				},
				{
					.buffer = ubos.back().buffer,
					.offset = sun_offset,
					.range = sizeof(SunData),
				},
				{
					.buffer = ubos.back().buffer,
					.offset = atmos_offset,
					.range = sizeof(AtmosphereInfo),
				}
			};

			vk::DescriptorImageInfo image_info = {
				.sampler = transmittance_sampler,
				.imageView = transmittance_view,
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
				.pImageInfo = &image_info,
			};

			device.device.updateDescriptorSets({ wd0, wd1, wd2, wd3 }, {});
		};
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
		.assymetry_mei = atmosphere_info.assymetry_mei,
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

			INFO_IF(result == vk::Result::eSuboptimalKHR, stl::fmt("Swapchain %s suboptimal", swapchain.name.data()))
			ELSE_IF_INFO(result == vk::Result::eErrorOutOfDateKHR, "Recreating Swapchain " + swapchain.name) DO(swapchain.recreate()) DO(recreate_framebuffers()) DO(Gui::Recreate())
			ELSE_IF_ERROR(failed(result), stl::fmt("Image acquire failed with %s", to_cstring(result))) THEN_CRASH(result)
			ELSE_VERBOSE("Image Acquired");
		}

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
					calculate_transmittance_lut(&device, &pipeline_factory, atmosphere_info, &transmittance_lut);
				}
			}

			Gui::End();

			Gui::EndBuild();
		}

		{
			OPTICK_EVENT("Image wait");
			result = device.device.waitForFences({ in_flight_frames[image_idx]->in_flight_fence }, true, max_value<u64>);
			ERROR_IF(failed(result), stl::fmt("Fence wait failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Fence Waited for");
			in_flight_frames[image_idx] = current_frame;
		}

		{
			OPTICK_EVENT("Ubo Update");
			camera_controller.update();
			camera.update();

			stl::vector<u8> buf_data(ubos[frame_idx].size);
			memcpy(buf_data.data(), &camera, sizeof(Camera));
			memcpy(buf_data.data() + closest_multiple(sizeof(Camera), 256), &sun, sizeof(SunData));	// TODO Fix hardcode
			memcpy(buf_data.data() + closest_multiple(sizeof(Camera), 256) + closest_multiple(sizeof(SunData), 256), &atmosphere_info, sizeof(AtmosphereInfo));	// TODO Fix hardcode
			device.update_data(&ubos[frame_idx], stl::span<u8>(buf_data.data(), buf_data.size()));
		}

		{
			OPTICK_EVENT("Reset Command Pool");
			device.device.resetCommandPool(current_frame->command_pool, {});
		}
		vk::CommandBuffer cmd = current_frame->command_buffer;

		result = cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, });
		ERROR_IF(failed(result), stl::fmt("Cmd Buffer begin failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_VERBOSE("Start Cmd Buffer");

		vk::ClearValue clear_val(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
		// Record Commands

		cmd.setViewport(0, { {
			.x = 0,
			.y = cast<f32>(swapchain.extent.height),
			.width = cast<f32>(swapchain.extent.width),
			.height = -cast<f32>(swapchain.extent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		} });
		cmd.setScissor(0, { {
			.offset = {0, 0},
			.extent = swapchain.extent,
		} });

		cmd.beginDebugUtilsLabelEXT({
			.pLabelName = "Triangle Draw",
			.color = std::array{0.0f, 0.5f, 0.0f, 1.0f},
		});

		cmd.beginRenderPass({
			.renderPass = renderpass.renderpass,
			.framebuffer = framebuffers[image_idx],
			.renderArea = {
				.offset = {0, 0},
				.extent = swapchain.extent,
			},
			.clearValueCount = 1,
			.pClearValues = &clear_val,
		}, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->pipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->layout->layout, 0, { descriptor_sets[frame_idx] }, {});
		cmd.draw(4, 1, 0, 0);

		cmd.endRenderPass();

		cmd.endDebugUtilsLabelEXT();

		Gui::Draw(cmd, image_idx);

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
			ELSE_IF_INFO(result == vk::Result::eErrorOutOfDateKHR, "Recreating Swapchain " + swapchain.name) DO(swapchain.recreate()) DO(recreate_framebuffers()) DO(Gui::Recreate())
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
		buf.destroy();
	}

	device.device.destroyDescriptorPool(descriptor_pool);

	pipeline->destroy();
	for (auto& framebuffer_ : framebuffers) {
		device.device.destroyFramebuffer(framebuffer_);
	}
	renderpass.destroy();

	device.device.destroySampler(transmittance_sampler);
	device.device.destroyImageView(transmittance_view);

	lut_pipeline->destroy();
	transmittance_lut.destroy();

	Gui::Destroy();
	camera_controller.destroy();
	camera.destroy();
	swapchain.destroy();
	device.destroy();
	window.destroy();
	context.destroy();
	glfw.destroy();

	return 0;
}

void calculate_transmittance_lut(Device* _device, PipelineFactory* _pipeline_factory, const AtmosphereInfo& _atmos, Image* _image) {
	Pipeline* pipeline;
	vk::RenderPass renderpass;
	vk::Framebuffer framebuffer;
	vk::ImageView lutview;
	vk::CommandBuffer cmd;
	vk::Fence fence;
	vk::Result result;

	tie(result, lutview) = _device->device.createImageView({
		.image = _image->image,
		.viewType = vk::ImageViewType::e2D,
		.format = _image->format,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.levelCount = 1,
			.layerCount = 1,
		},
	});
	ERROR_IF(failed(result), stl::fmt("LUT Image View could not be created with %s", to_cstring(result)));
	_device->set_object_name(lutview, _image->name + " view");

	vk::AttachmentDescription attach_desc = {
		.format = _image->format,
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
	tie(result, renderpass) = _device->device.createRenderPass({
		.attachmentCount = 1,
		.pAttachments = &attach_desc,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	});
	ERROR_IF(failed(result), stl::fmt("Renderpass creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Renderpass Created");
	_device->set_object_name(renderpass, "Transmittance LUT pass");

	// Framebuffer
	tie(result, framebuffer) = _device->device.createFramebuffer(vk::FramebufferCreateInfo{
		.renderPass = renderpass,
		.attachmentCount = 1,
		.pAttachments = &lutview,
		.width = _image->extent.width,
		.height = _image->extent.height,
		.layers = 1,
		});
	ERROR_IF(failed(result), stl::fmt("LUT Framebuffer creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Framebuffer created");
	_device->set_object_name(framebuffer, "Transmittance LUT Framebuffer");

	if (lut_pipeline == nullptr) {
		tie(result, lut_pipeline) = _pipeline_factory->create_pipeline({
			.renderpass = renderpass,
			.viewport_state = {
				.enable_dynamic = false,
				.viewports = {
					{
						.x = 0.0f,
						.y = 0.0f,
						.width = cast<f32>(_image->extent.width),
						.height = cast<f32>(_image->extent.height),
						.minDepth = 0.0f,
						.maxDepth = 1.0f,
					}
				},
				.scissors = {
					{
						.offset = {0, 0},
						.extent = {_image->extent.width, _image->extent.height},
					}
				}
			},
			.raster_state = {
				.front_face = vk::FrontFace::eCounterClockwise,
			},
			.shader_files = { R"(res/shaders/transmittance_lut.vs.spv)", R"(res/shaders/transmittance_lut.fs.spv)" },
			.name = "LUT Pipeline",
		});
		ERROR_IF(failed(result), stl::fmt("LUT Pipeline creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("LUT Pipeline Created");
	}

	tie(result, pipeline) = _pipeline_factory->create_pipeline({
		.renderpass = renderpass,
		.viewport_state = {
			.enable_dynamic = false,
			.viewports = {
				{
					.x = 0.0f,
					.y = 0.0f,
					.width = cast<f32>(_image->extent.width),
					.height = cast<f32>(_image->extent.height),
					.minDepth = 0.0f,
					.maxDepth = 1.0f,
				}
			},
			.scissors = {
				{
					.offset = {0, 0},
					.extent = {_image->extent.width, _image->extent.height},
				}
			}
		},
		.raster_state = {
			.front_face = vk::FrontFace::eCounterClockwise,
		},
		.shader_files = { R"(res/shaders/transmittance_lut.vs.spv)", R"(res/shaders/transmittance_lut.fs.spv)" },
		.name = "LUT Pipeline",
	});
	ERROR_IF(failed(result), stl::fmt("LUT Pipeline creation failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("LUT Pipeline Created");

	rdoc::start_capture();

	tie(result, cmd) = _device->alloc_temp_command_buffer(_device->graphics_cmd_pool);

	result = cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit, });
	ERROR_IF(failed(result), stl::fmt("Command buffer begin failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Cmd Created");

	cmd.beginDebugUtilsLabelEXT({
		.pLabelName = "Transmittance LUT Calculation",
		.color = std::array{0.5f, 0.0f, 0.0f, 1.0f},
	});

	vk::ClearValue clear_val(std::array{ 0.0f, 1.0f, 0.0f, 1.0f });
	cmd.beginRenderPass({
		.renderPass = renderpass,
		.framebuffer = framebuffer,
		.renderArea = {
			.offset = {0, 0},
			.extent = {_image->extent.width, _image->extent.height},
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
	ERROR_IF(failed(result), stl::fmt("Command buffer end failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("Command buffer Created");

	SubmitTask<void> task;
	result = task.submit(_device, _device->queues.graphics, _device->graphics_cmd_pool, { cmd });
	ERROR_IF(failed(result), stl::fmt("Submit failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("LUT Submitted Created");

	result = task.wait();
	ERROR_IF(failed(result), stl::fmt("Fence waiting failed with %s", to_cstring(result))) THEN_CRASH(result) ELSE_INFO("LUT Written to");

	rdoc::end_capture();

	_device->device.destroyFence(fence);
	_device->device.destroyImageView(lutview);
	pipeline->destroy();
	_device->device.destroyFramebuffer(framebuffer);
	_device->device.destroyRenderPass(renderpass);
}