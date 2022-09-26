// =============================================
//  Aster: renderpass.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>
#include <core/device.h>

struct RenderPass {
	vk::RenderPass renderpass;
	name_t name;
	usize attachment_format;
	Device* parent_device;

	static Result<RenderPass, vk::Result> create(const std::string& _name, Device* _device, const vk::RenderPassCreateInfo& _create_info);

	void destroy();
};
