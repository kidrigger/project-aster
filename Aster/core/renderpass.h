/*=========================================*/
/*  Aster: core/renderpass.h               */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>
#include <core/device.h>

struct RenderPass {
	vk::RenderPass renderpass;
	stl::string name;
	usize attachment_format;
	Device* parent_device;

	static vk::ResultValue<RenderPass> create(const stl::string& _name, Device* _device, const vk::RenderPassCreateInfo& _create_info);

	void destroy();
};
