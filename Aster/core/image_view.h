// =============================================
//  Aster: image_view.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once
#include <global.h>
#include <core/image.h>

struct ImageView {
	Borrowed<Image> parent_image;
	vk::ImageView image_view;
	vk::Format format;
	vk::ImageViewType type;
	vk::ImageSubresourceRange subresource_range;
	std::string name;

	ImageView() = default;

	ImageView(const Borrowed<Image>& _parent_image, const vk::ImageView& _image_view, vk::Format _format, vk::ImageViewType _type, const vk::ImageSubresourceRange& _subresource_range, const std::string& _name);
	ImageView(const ImageView& _other) = delete;
	ImageView(ImageView&& _other) noexcept;
	ImageView& operator=(const ImageView& _other) = delete;
	ImageView& operator=(ImageView&& _other) noexcept;

	static Res<ImageView> create(const Borrowed<Image>& _image, vk::ImageViewType _image_type, const vk::ImageSubresourceRange& _subresource_range);

	~ImageView();
};
