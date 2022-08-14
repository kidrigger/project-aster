// =============================================
//  Aster: image_view.cc
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#include "image_view.h"
#include <core/image.h>
#include <core/device.h>

vk::ResultValue<ImageView> ImageView::create(Image* _image, const vk::ImageViewType _image_type, const vk::ImageSubresourceRange& _subresource_range) {

	auto [result, image_view] = _image->parent_device->device.createImageView({
		.image = _image->image,
		.viewType = _image_type,
		.format = _image->format,
		.subresourceRange = _subresource_range,
	});

	const auto name = std::fmt("%s view", _image->name.c_str());

	if (!failed(result)) {
		_image->parent_device->set_object_name(image_view, name);
	}

	return vk::ResultValue<ImageView>(result, {
		.parent_image = _image,
		.parent_device = _image->parent_device,
		.image_view = image_view,
		.format = _image->format,
		.type = _image_type,
		.subresource_range = _subresource_range,
		.name = name
	});
}

void ImageView::destroy() {
	if (parent_device && image_view) {
		parent_device->device.destroyImageView(image_view);
		parent_image = nullptr;
		parent_device = nullptr;
		image_view = nullptr;
	}
}
