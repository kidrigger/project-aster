// =============================================
//  Aster: image_view.cpp
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "image_view.h"
#include <core/device.h>

ImageView::ImageView(const Borrowed<Image>& _parent_image, const vk::ImageView& _image_view, vk::Format _format, vk::ImageViewType _type, const vk::ImageSubresourceRange& _subresource_range, const std::string& _name): parent_image(_parent_image)
                                                                                                                                                                                                                        , image_view(_image_view)
                                                                                                                                                                                                                        , format(_format)
                                                                                                                                                                                                                        , type(_type)
                                                                                                                                                                                                                        , subresource_range(_subresource_range)
                                                                                                                                                                                                                        , name(_name) {}

ImageView::ImageView(ImageView&& _other) noexcept: parent_image{ std::move(_other.parent_image) }
                                                 , image_view{ std::exchange(_other.image_view, nullptr) }
                                                 , format{ _other.format }
                                                 , type{ _other.type }
                                                 , subresource_range{ _other.subresource_range }
                                                 , name{ std::move(_other.name) } {}

ImageView& ImageView::operator=(ImageView&& _other) noexcept {
	if (this == &_other) return *this;
	std::swap(parent_image, _other.parent_image);
	std::swap(image_view, _other.image_view);
	format = _other.format;
	type = _other.type;
	subresource_range = _other.subresource_range;
	name = std::move(_other.name);
	return *this;
}

Res<ImageView> ImageView::create(const Borrowed<Image>& _image, vk::ImageViewType _image_type, const vk::ImageSubresourceRange& _subresource_range) {

	auto [result, image_view] = _image->parent_device->device.createImageView({
		.image = _image->image,
		.viewType = _image_type,
		.format = _image->format,
		.subresourceRange = _subresource_range,
	});

	if (failed(result)) {
		return Err::make(std::fmt("ImageView '%s view' creation failed with %s" CODE_LOC, _image->name.c_str(), to_cstr(result)), result);
	}

	const auto name = std::fmt("%s view", _image->name.c_str());
	_image->parent_device->set_object_name(image_view, name);

	return ImageView{
		_image,
		image_view,
		_image->format,
		_image_type,
		_subresource_range,
		name
	};
}

ImageView::~ImageView() {
	if (image_view) {
		parent_image->parent_device->device.destroyImageView(image_view);
	}
}
