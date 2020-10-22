/*=========================================*/
/*  Aster: source.cc                       */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include <global.h>
#include <logger.h>
#include <cstdio>

#include <core/glfw_context.h>
#include <core/window.h>
#include <core/context.h>
#include <core/device.h>
#include <core/swapchain.h>

#include <EASTL/list.h>

int main() {

	g_logger.set_minimum_logging_level(Logger::LogType::eVerbose);

	GLFWContext glfw;
	Context context;
	Window window;
	Device device;
	Swapchain swapchain;

	glfw.init();
	context.init("Aster Core", Version{ 0, 0, 1 });
	window.init(&context, 640u, 480u, PROJECT_NAME, false);
	device.init("Primary", &context, &window);
	swapchain.init(window.name, &window, &device);

	while (window.poll()) {

	}

	swapchain.destroy();
	device.destroy();
	window.destroy();
	context.destroy();
	glfw.destroy();

	return 0;
}
