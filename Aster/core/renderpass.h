// =============================================
//  Aster: renderpass.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>
#include <core/device.h>

struct RenderPass {
	vk::RenderPass renderpass;
	std::string name;
	usize attachment_format;
	Borrowed<Device> parent_device;

	static vk::ResultValue<RenderPass> create(const std::string& _name, const Borrowed<Device>& _device, const vk::RenderPassCreateInfo& _create_info);

	void destroy();
};
