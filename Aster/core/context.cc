/*=========================================*/
/*  Aster: core/context.cc                 */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include "context.h"

VKAPI_ATTR b32 VKAPI_CALL Context::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT _message_severity, VkDebugUtilsMessageTypeFlagsEXT _message_type, const VkDebugUtilsMessengerCallbackDataEXT* _callback_data, [[maybe_unused]] void* _user_data) {
	using Severity = vk::DebugUtilsMessageSeverityFlagsEXT;
	using SeverityBits = vk::DebugUtilsMessageSeverityFlagBitsEXT;
	using MessageType = vk::DebugUtilsMessageTypeFlagsEXT;
	using MessageTypeBits = vk::DebugUtilsMessageTypeFlagBitsEXT;

	auto severity = Severity(_message_severity);
	auto message_type = MessageType(_message_type);

	if (message_type & MessageTypeBits::eValidation) {
		if (severity & SeverityBits::eError)
			ERROR(_callback_data->pMessage);
		if (severity & SeverityBits::eWarning)
			WARN(_callback_data->pMessage);
		if (severity & SeverityBits::eInfo)
			INFO(_callback_data->pMessage);
		if (severity & SeverityBits::eVerbose)
			VERBOSE(_callback_data->pMessage);
	}

	return false;
}

void Context::init(const std::string_view& _app_name, const Version& _app_version) {
	vk::Result result;

	INFO_IF(enable_validation_layers, "Validation Layers enabled");

	// Creating Instance
	vk::ApplicationInfo app_info = {
		.pApplicationName = _app_name.data(),
		.applicationVersion = VK_MAKE_VERSION(_app_version.major, _app_version.minor, _app_version.patch),
		.pEngineName = PROJECT_NAME,
		.engineVersion = VK_MAKE_VERSION(VERSION.major, VERSION.minor, VERSION.patch),
		.apiVersion = VK_API_VERSION_1_2,
	};

	vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
		.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo,
		.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
		.pfnUserCallback = debug_callback,
		.pUserData = nullptr,
	};

	u32 glfw_extension_count = 0;
	const char** glfw_extensions = nullptr;

	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
	std::vector<const char*> vulkan_extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
	if (enable_validation_layers) {
		vulkan_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	vk::DynamicLoader dl;
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	tie(result, instance) = vk::createInstance({
		.pNext = enable_validation_layers ? &debug_messenger_create_info : nullptr,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = enable_validation_layers ? cast<u32>(validation_layers.size()) : 0,
		.ppEnabledLayerNames = enable_validation_layers ? validation_layers.data() : nullptr,
		.enabledExtensionCount = cast<u32>(vulkan_extensions.size()),
		.ppEnabledExtensionNames = vulkan_extensions.data(),
	});
	ERROR_IF(failed(result), "Failed to create Vulkan instance with "s + to_string(result)) THEN_CRASH(result) ELSE_INFO("Instance Created.");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

	// Debug Messenger
	if (enable_validation_layers) {
		tie(result, debug_messenger) = instance.createDebugUtilsMessengerEXT(debug_messenger_create_info);
		ERROR_IF(failed(result), "Debug Messenger creation failed with "s + to_string(result)) ELSE_INFO("Debug Messenger Created.");
	}
}

Context::~Context() {
	if (instance) {
		if (enable_validation_layers && debug_messenger) {
			instance.destroyDebugUtilsMessengerEXT(debug_messenger);
		}
		instance.destroy();
		INFO("Context destroyed");
	}
}
