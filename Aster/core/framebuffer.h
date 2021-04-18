// =============================================
//  Aster: framebuffer.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>


#include "image_view.h"
#include "renderpass.h"

class Framebuffer {
public:
	static Res<Framebuffer> create(const std::string_view& _name, const Borrowed<RenderPass>& _render_pass, const std::vector<Borrowed<ImageView>>& _attachments, const u32 _layer_count) {
		auto extent = _attachments.front()->parent_image->extent;
		auto layer_count = _layer_count;
		
		std::vector<vk::ImageView> image_views;
		image_views.reserve(_attachments.size());
		
		for(const auto& attach : _attachments) {
			if (attach->parent_image->extent != extent) {
				return Err::make(std::fmt("Attachment '%s' has extent '(%d, %d)' which is not equal to previous '(%d, %d)'", attach->name.c_str(), attach->parent_image->extent.width, attach->parent_image->extent.height, extent.width, extent.height));
			}
			image_views.push_back(attach->image_view);
		}

		auto [result, framebuffer] = _render_pass->parent_device->device.createFramebuffer(vk::FramebufferCreateInfo{
			.renderPass = _render_pass->renderpass,
			.width = extent.width,
			.height = extent.height,
			.layers = layer_count
			}.setAttachments(image_views));
		if (failed(result)) {
			return Err::make(std::fmt("Framebuffer creation failed with %s" CODE_LOC, to_cstr(result), result));
		}
		
		return Framebuffer{
			std::string{_name},
			framebuffer,
			_render_pass->parent_device,
			_render_pass->attachment_format,
			{ extent.width, extent.height },
			cast<u32>(_attachments.size()),
		};
	}

	Framebuffer() = default;
	
	Framebuffer(const std::string_view& _name, const vk::Framebuffer& _framebuffer, const Borrowed<Device>& _parent_device, const usize _rp_attachment_format, const vk::Extent2D _extent, const u32 _attachment_count)
		: framebuffer(_framebuffer)
		, parent_device(_parent_device)
		, rp_attachment_format(_rp_attachment_format)
		, extent(_extent)
		, attachment_count(_attachment_count)
		, name(_name) {
		parent_device->set_object_name(_framebuffer, name.c_str());
	}

	Framebuffer(const Framebuffer& _other) = delete;

	Framebuffer(Framebuffer&& _other) noexcept
		: framebuffer{ std::exchange(_other.framebuffer, nullptr) }
		, parent_device{ std::move(_other.parent_device) }
		, rp_attachment_format{ _other.rp_attachment_format }
		, extent{_other.extent}
		, attachment_count{ _other.attachment_count } {}

	Framebuffer& operator=(const Framebuffer& _other) = delete;

	Framebuffer& operator=(Framebuffer&& _other) noexcept {
		if (this == &_other) return *this;
		framebuffer = std::exchange(_other.framebuffer, nullptr);
		parent_device = std::move(_other.parent_device);
		rp_attachment_format = _other.rp_attachment_format;
		extent = _other.extent;
		attachment_count = _other.attachment_count;
		return *this;
	}

	~Framebuffer() {
		parent_device->device.destroyFramebuffer(framebuffer);
	}

	// fields
	vk::Framebuffer framebuffer;
	Borrowed<Device> parent_device;
	usize rp_attachment_format;
	vk::Extent2D extent;
	u32 attachment_count;
	std::string name;
};
