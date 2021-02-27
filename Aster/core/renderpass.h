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

	RenderPass() = default;

	RenderPass(const vk::RenderPass& _renderpass, const std::string_view& _name, usize _attachment_format, const Borrowed<Device>& _parent_device)
		: renderpass(_renderpass)
		, name(_name)
		, attachment_format(_attachment_format)
		, parent_device(_parent_device) {}

	RenderPass(const RenderPass& _other) = delete;

	RenderPass(RenderPass&& _other) noexcept
		: renderpass{ std::exchange(_other.renderpass, nullptr) }
		, name{ std::move(_other.name) }
		, attachment_format{ _other.attachment_format }
		, parent_device{ std::move(_other.parent_device) } {}

	RenderPass& operator=(const RenderPass& _other) = delete;

	RenderPass& operator=(RenderPass&& _other) noexcept {
		if (this == &_other) return *this;
		std::swap(renderpass, _other.renderpass);
		name = std::move(_other.name);
		attachment_format = _other.attachment_format;
		std::swap(parent_device, _other.parent_device);
		return *this;
	}

	static Res<RenderPass> create(const std::string_view& _name, const Borrowed<Device>& _device, const vk::RenderPassCreateInfo& _create_info);

	~RenderPass();
};
