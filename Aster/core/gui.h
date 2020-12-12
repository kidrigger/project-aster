/*=========================================*/
/*  Aster: core/gui.h                      */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#pragma warning(push, 0)
#include <imgui/imgui.h>
#pragma warning(pop)

#include <core/context.h>
#include <core/swapchain.h>

namespace ImGui {
	// Following ImGui Style
	void Init(Swapchain* swapchain);
	void Destroy();

	void Recreate();
	void StartBuild();
	void EndBuild();
	void Draw(vk::CommandBuffer cmd, i32 image_idx);

	void PushDisable();
	void PopDisable();
}

namespace Gui = ImGui;