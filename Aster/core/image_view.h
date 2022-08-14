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
	std::string name;

	static vk::ResultValue<ImageView> create(Image* _image, vk::ImageViewType _image_type, const vk::ImageSubresourceRange& _subresource_range);

	void destroy();
};
