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
 * Sets up the devices, surface, extensions, layers, commandpools, and quieues.
 */
struct Context final {

	stl::vector<const char*> validation_layers = {
		"VK_LAYER_KHRONOS_validation",
	};

	stl::vector<const char*> device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MULTIVIEW_EXTENSION_NAME,
	};

	void init(const stl::string& app_name, const Version& app_version) noexcept;
	void destroy() noexcept;

	static VKAPI_ATTR b32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	                                                VkDebugUtilsMessageTypeFlagsEXT message_type,
	                                                const VkDebugUtilsMessengerCallbackDataEXT* _callback_data,
	                                                void* user_data);

	// Fields
	bool enable_validation_layers{ true };
	bool is_complete{ false };

	vk::Instance instance;
	vk::DebugUtilsMessengerEXT debug_messenger;

};
