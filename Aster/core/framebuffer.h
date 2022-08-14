// =============================================
//  Aster: framebuffer.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>


#include <core/image_view.h>
#include <core/image.h>
#include <core/renderpass.h>

struct Framebuffer {
	vk::Framebuffer framebuffer;
	Device* parent_device;
	usize rp_attachment_format;
	vk::Extent2D extent;
	u32 attachment_count;
	std::string name;

	static vk::ResultValue<Framebuffer> create(const std::string_view& _name, const RenderPass* _render_pass, const std::vector<ImageView>& _attachments, const u32 _layer_count) {

		auto* const parent_device = _render_pass->parent_device;
		auto extent = _attachments.front().parent_image->extent;
		auto layer_count = _layer_count;

		std::vector<vk::ImageView> image_views;
		image_views.reserve(_attachments.size());

		for (const auto& attach : _attachments) {
			ERROR_IF(attach.parent_image->extent != extent, std::fmt("Attachment '%s' has extent '(%d, %d)' which is not equal to previous '(%d, %d)'", attach.name.c_str(), attach.parent_image->extent.width, attach.parent_image->extent.height, extent.width, extent.height)) THEN_CRASH(-1);
			image_views.push_back(attach.image_view);
		}

		auto [result, framebuffer] = _render_pass->parent_device->device.createFramebuffer(vk::FramebufferCreateInfo{
			.renderPass = _render_pass->renderpass,
			.width = extent.width,
			.height = extent.height,
			.layers = layer_count
		}.setAttachments(image_views));

		if (!failed(result)) {
			parent_device->set_object_name(framebuffer, _name);
		}

		return vk::ResultValue(result, Framebuffer{
			framebuffer,
			parent_device,
			_render_pass->attachment_format,
			{ extent.width, extent.height },
			cast<u32>(_attachments.size()),
			std::string{ _name },
		});
	}

	void destroy() {
		if (parent_device && framebuffer) {
			parent_device->device.destroyFramebuffer(framebuffer);
			parent_device = nullptr;
			framebuffer = nullptr;
		}
	}
};
