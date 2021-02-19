/*=========================================*/
/*  Aster: core/context.h                  */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>
#include <array>
#include <vector>

/**
 * @class Context
 *
 * @brief Vulkan context to handle device initialization logic.
 *
 * Handles the required hardware interactions.
 */
class Context final {
public:
	Context(const std::string_view& _app_name, const Version& _app_version, const b8 _enable_validation = true) : enable_validation_layers{ _enable_validation } {
		init(_app_name, _app_version);
	}

	Context(const std::string_view& _app_name, const Version& _app_version, const std::vector<const char*>& _additional_device_extensions, const b8 _enable_validation = true) : enable_validation_layers{ _enable_validation } {
		device_extensions.reserve(device_extensions.size() + _additional_device_extensions.size());
		device_extensions.insert(device_extensions.end(), _additional_device_extensions.begin(), _additional_device_extensions.end());

		init(_app_name, _app_version);
	}

	Context(const std::string_view& _app_name, const Version& _app_version, const std::vector<const char*>& _additional_device_extensions, const std::vector<const char*>& _additional_validation_layers) {
		device_extensions.reserve(device_extensions.size() + _additional_device_extensions.size());
		device_extensions.insert(device_extensions.end(), _additional_device_extensions.begin(), _additional_device_extensions.end());

		validation_layers.reserve(validation_layers.size() + _additional_validation_layers.size());
		validation_layers.insert(validation_layers.end(), _additional_validation_layers.begin(), _additional_validation_layers.end());

		init(_app_name, _app_version);
	}

	Context(const Context& _other) = delete;

	Context(Context&& _other) noexcept
		: enable_validation_layers{ _other.enable_validation_layers }
		, validation_layers{ std::move(_other.validation_layers) }
		, device_extensions{ std::move(_other.device_extensions) }
		, instance{ std::exchange(_other.instance, nullptr) }
		, debug_messenger{ std::exchange(_other.debug_messenger, nullptr) } {}

	Context& operator=(const Context& _other) = delete;

	Context& operator=(Context&& _other) noexcept {
		if (this == &_other) return *this;
		enable_validation_layers = _other.enable_validation_layers;
		validation_layers = std::move(_other.validation_layers);
		device_extensions = std::move(_other.device_extensions);
		instance = std::exchange(_other.instance, nullptr);
		debug_messenger = std::exchange(_other.debug_messenger, nullptr);
		return *this;
	}

	~Context();

	// Fields
	bool enable_validation_layers{ true };

	std::vector<const char*> validation_layers = {
		"VK_LAYER_KHRONOS_validation",
	};

	std::vector<const char*> device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MULTIVIEW_EXTENSION_NAME,
	};

	vk::Instance instance;
	vk::DebugUtilsMessengerEXT debug_messenger;

private:
	void init(const std::string_view& _app_name, const Version& _app_version);

	static VKAPI_ATTR b32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT _message_severity,
	                                                VkDebugUtilsMessageTypeFlagsEXT _message_type,
	                                                const VkDebugUtilsMessengerCallbackDataEXT* _callback_data,
	                                                void* _user_data);
};
