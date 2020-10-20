/*=========================================*/
/*  Aster: source.cc                       */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include <global.h>
#include <logger.h>
#include <cstdio>

#include <main/glfw_context.h>
#include <main/window.h>
#include <main/context.h>
#include <main/device.h>
#include <main/swapchain.h>

#include <EASTL/list.h>

int main() {

	g_logger.set_minimum_logging_level(Logger::LogType::eVerbose);

	GLFWContext glfw;
	glfw.init();

	Context context;
	context.init("Aster Core", Version{ 0, 0, 1 });

	//stl::vector<Window*> windows;
	//windows.emplace_back(new Window())->init(&context, 640u, 480u, PROJECT_NAME, false);
	Window window;
	window.init(&context, 640u, 480u, PROJECT_NAME, false);

	Device device;
	device.init("Primary", &context, &window);

	Swapchain swapchain;
	swapchain.init(window.name, &window, &device);

	//b8 to_delete = false;
	while (window.poll()) {

		//for (auto& w : windows) {
		//	if (!w->poll()) {
		//		stl::swap(w, windows.back());
		//		w->poll();
		//		to_delete = true;
		//	}
		//}
		//if (to_delete) {
		//	windows.back()->destroy();
		//	delete windows.back();
		//	windows.pop_back();
		//	to_delete = false;
		//}
	}

	swapchain.destroy();

	device.destroy();
	//for (auto& w : windows) {
	//	w->destroy();
	//	delete w;
	//}
	//windows.clear();
	window.destroy();
	context.destroy();

	glfw.destroy();

	return 0;
}
