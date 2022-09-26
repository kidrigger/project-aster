// =============================================
//  Aster: image_view.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>

struct Image;
class Device;

struct ImageView {
	Image* parent_image;
	Device* parent_device;
	vk::ImageView image_view;
	vk::Format format;
	vk::ImageViewType type;
	vk::ImageSubresourceRange subresource_range;
	name_t name;

	static Result<ImageView, vk::Result> create(Image* _image, vk::ImageViewType _image_type, const vk::ImageSubresourceRange& _subresource_range);

	void destroy();
};
