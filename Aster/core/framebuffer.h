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
	name_t name;

	static Result<Framebuffer, vk::Result> create(const std::string_view& _name, const RenderPass* _render_pass, const std::vector<ImageView>& _attachments, u32 _layer_count);

	void destroy();
};
