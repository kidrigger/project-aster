// =============================================
//  Aster: gui.cc
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "gui.h"

#pragma warning(push, 0)
#include <imgui/imgui_internal.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>
#pragma warning(pop)

#include <optick/optick.h>

namespace ImGui {
	vk::DescriptorPool descriptor_pool;
	vk::RenderPass renderpass;
	std::vector<vk::Framebuffer> framebuffers;

	Borrowed<Swapchain> current_swapchain;

	void vulkan_assert(VkResult _res) {
		auto result = cast<vk::Result>(_res);
		ERROR_IF(failed(result), std::fmt("Assert failed with %s", to_cstr(result))) THEN_CRASH(result);
	}

	void Init(const Borrowed<Swapchain>& _swapchain) {
		current_swapchain = _swapchain;

		auto& device_ = current_swapchain->parent_device;

		vk::Result result;

		std::vector<vk::DescriptorPoolSize> pool_sizes = {
			{ vk::DescriptorType::eSampler, 1000 },
			{ vk::DescriptorType::eCombinedImageSampler, 1000 },
			{ vk::DescriptorType::eSampledImage, 1000 },
			{ vk::DescriptorType::eStorageImage, 1000 },
			{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
			{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
			{ vk::DescriptorType::eUniformBuffer, 1000 },
			{ vk::DescriptorType::eStorageBuffer, 1000 },
			{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
			{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
			{ vk::DescriptorType::eInputAttachment, 1000 }
		};

		tie(result, descriptor_pool) = device_->device.createDescriptorPool({
			.maxSets = 1000,
			.poolSizeCount = cast<u32>(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data(),
		});
		ERROR_IF(failed(result), std::fmt("Descriptor pool creation failed with %s", result)) THEN_CRASH(result);
		device_->set_object_name(descriptor_pool, "Imgui descriptor pool"s);


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

		vk::AttachmentDescription attach_desc = {
			.format = _swapchain->format,
			.loadOp = vk::AttachmentLoadOp::eLoad,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
			.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
			.initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.finalLayout = vk::ImageLayout::ePresentSrcKHR,
		};

		// Renderpass
		tie(result, renderpass) = device_->device.createRenderPass({
			.attachmentCount = 1,
			.pAttachments = &attach_desc,
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = 1,
			.pDependencies = &dependency
		});
		ERROR_IF(failed(result), std::fmt("Renderpass creation failed with %s", to_cstr(result))) THEN_CRASH(result) ELSE_INFO("UI pass Created");
		device_->set_object_name(renderpass, "UI pass");

		IMGUI_CHECKVERSION();
		CreateContext();
		ImGuiIO& io = GetIO();
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Viewports bad

		StyleColorsDark();

		ImGui_ImplGlfw_InitForVulkan(_swapchain->parent_window->window, true);

		ImGui_ImplVulkan_InitInfo init_info = {
			.Instance = device_->parent_context->instance,
			.PhysicalDevice = device_->physical_device,
			.Device = device_->device,
			.QueueFamily = device_->queue_families.graphics_idx,
			.Queue = device_->queues.graphics,
			.PipelineCache = nullptr,
			.DescriptorPool = descriptor_pool,
			.MinImageCount = _swapchain->image_count,
			.ImageCount = _swapchain->image_count,
			.Allocator = nullptr,
			.CheckVkResultFn = vulkan_assert,
		};
		ImGui_ImplVulkan_Init(&init_info, renderpass);

		vk::CommandBuffer cmd;
		tie(result, cmd) = device_->alloc_temp_command_buffer(device_->transfer_cmd_pool);
		result = cmd.begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		ImGui_ImplVulkan_CreateFontsTexture(cmd);

		result = cmd.end();
		ERROR_IF(failed(result), std::fmt("Cmd buffer end failed with %s", to_cstr(result))) THEN_CRASH(result);

		auto task = SubmitTask<void>();
		result = task.submit(device_, device_->queues.transfer, device_->transfer_cmd_pool, { cmd });
		ERROR_IF(failed(result), std::fmt("Fonts could not be loaded to GPU with %s", to_cstr(result))) THEN_CRASH(result);

		framebuffers.reserve(_swapchain->image_count);
		for (const auto& iv : _swapchain->image_views) {
			tie(result, framebuffers.emplace_back()) = device_->device.createFramebuffer({
				.renderPass = renderpass,
				.attachmentCount = 1,
				.pAttachments = &iv,
				.width = _swapchain->extent.width,
				.height = _swapchain->extent.height,
				.layers = 1,
			});
			ERROR_IF(failed(result), std::fmt("GUI Framebuffer creation failed with %s", to_cstr(result))) THEN_CRASH(result);
		}

		result = task.wait_and_destroy();
		ERROR_IF(failed(result), std::fmt("Fence wait failed with %s", to_cstr(result))) THEN_CRASH(result);
	}

	void Destroy() {
		current_swapchain->parent_device->device.destroyDescriptorPool(descriptor_pool);
		for (auto& fb : framebuffers) {
			current_swapchain->parent_device->device.destroyFramebuffer(fb);
		}
		current_swapchain->parent_device->device.destroyRenderPass(renderpass);
		current_swapchain = {};

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		DestroyContext();
	}

	void Recreate() {
		vk::Result result;
		for (auto& fb : framebuffers) {
			current_swapchain->parent_device->device.destroyFramebuffer(fb);
		}
		framebuffers.clear();
		framebuffers.reserve(current_swapchain->image_count);
		for (auto& iv : current_swapchain->image_views) {
			tie(result, framebuffers.emplace_back()) = current_swapchain->parent_device->device.createFramebuffer({
				.renderPass = renderpass,
				.attachmentCount = 1,
				.pAttachments = &iv,
				.width = current_swapchain->extent.width,
				.height = current_swapchain->extent.height,
				.layers = 1,
			});
			ERROR_IF(failed(result), std::fmt("GUI Framebuffer creation failed with %s", to_cstr(result))) THEN_CRASH(result);
		}
	}

	void StartBuild() {
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		NewFrame();

		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

		ImGuiViewport* viewport = GetMainViewport();
		SetNextWindowPos(viewport->GetWorkPos());
		SetNextWindowSize(viewport->GetWorkSize());
		SetNextWindowViewport(viewport->ID);
		PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |=
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		Begin("DockSpace Demo", nullptr, window_flags);
		PopStyleVar();

		PopStyleVar(2);

		// DockSpace
		if (GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			const ImGuiID dockspace_id = GetID("MyDockSpace");
			DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}
	}

	void EndBuild() {
		End();
		Render();

		EndFrame();
		if (GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			UpdatePlatformWindows();
			RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	void Draw(vk::CommandBuffer _cmd, i32 _image_idx) {
		OPTICK_EVENT();

		_cmd.beginDebugUtilsLabelEXT({
			.pLabelName = "UI pass",
			.color = std::array{ 0.0f, 0.0f, 1.0f, 1.0f },
		});
		_cmd.beginRenderPass({
			.renderPass = renderpass,
			.framebuffer = framebuffers[_image_idx],
			.renderArea = {
				.offset = { 0, 0 },
				.extent = current_swapchain->extent,
			},
			.clearValueCount = 0,
		}, vk::SubpassContents::eInline);

		ImGui_ImplVulkan_RenderDrawData(GetDrawData(), _cmd);

		_cmd.endRenderPass();
		_cmd.endDebugUtilsLabelEXT();
	}

	void PushDisable() {
		PushItemFlag(ImGuiItemFlags_Disabled, true);
		PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.5f);
	}

	void PopDisable() {
		PopStyleVar();
		PopItemFlag();
	}
}
