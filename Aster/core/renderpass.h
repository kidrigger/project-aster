// =============================================
//  Aster: renderpass.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>
#include <core/device.h>

struct RenderPass {
	vk::RenderPass renderpass;
	std::string name;
	usize attachment_format;
	Device* parent_device;

	static vk::ResultValue<RenderPass> create(const std::string& _name, Device* _device, const vk::RenderPassCreateInfo& _create_info);

	void destroy();
};
