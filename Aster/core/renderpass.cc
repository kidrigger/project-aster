/*=========================================*/
/*  Aster: core/renderpass.cc              */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include "renderpass.h"

vk::ResultValue<RenderPass> RenderPass::create(Device* _device, const stl::string& _name, const vk::RenderPassCreateInfo& _create_info) {
	ERROR_IF(_create_info.subpassCount != 1, stl::fmt("Renderpass %s has more than 1 subpass. Currently unsupported"));

	const auto attachments = stl::span(_create_info.pAttachments, cast<usize>(_create_info.attachmentCount));
	const auto color_refs = stl::span(_create_info.pSubpasses->pColorAttachments, cast<usize>(_create_info.pSubpasses->colorAttachmentCount));
	const auto preserve_refs = stl::span(_create_info.pSubpasses->pPreserveAttachments, cast<usize>(_create_info.pSubpasses->preserveAttachmentCount));

	usize hash_value = 0;

	for (const auto& attachment_ : attachments) {
		hash_value = hash_combine(hash_value, hash_any(attachment_.format));
		hash_value = hash_combine(hash_value, hash_any(attachment_.samples));
	}

	usize color_ref_map = 0;
	for (const auto& ref : color_refs) {
		color_ref_map |= (cast<usize>(1) << ref.attachment);
	}
	hash_value = hash_combine(hash_value, hash_any(color_ref_map));

	usize preserve_ref_map = 0;
	for (const auto& ref : preserve_refs) {
		preserve_ref_map |= (cast<usize>(1) << ref);
	}
	hash_value = hash_combine(hash_value, hash_any(preserve_ref_map));

	if (_create_info.pSubpasses->pDepthStencilAttachment != nullptr) {
		hash_value = hash_combine(hash_value, hash_any(_create_info.pSubpasses->pDepthStencilAttachment->attachment));
	}

	auto [result, rp] = _device->device.createRenderPass(_create_info);
	if (!failed(result)) {
		auto renderpass = RenderPass{
			.renderpass = rp,
			.name = _name,
			.attachment_format = hash_value,
			.parent_device = _device,
		};
		_device->set_object_name(renderpass.renderpass, _name);
		return { result, renderpass };
	}
	return { result, {} };
}

void RenderPass::destroy() {
	parent_device->device.destroyRenderPass(renderpass);
}
