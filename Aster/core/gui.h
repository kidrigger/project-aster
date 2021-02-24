// =============================================
//  Aster: gui.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#pragma warning(push, 0)
#include <imgui/imgui.h>
#pragma warning(pop)

#include <core/swapchain.h>

namespace ImGui {
	// Following ImGui Style
	void Init(const Borrowed<Swapchain>& _swapchain);
	void Destroy();

	void Recreate();
	void StartBuild();
	void EndBuild();
	void Draw(vk::CommandBuffer _cmd, i32 _image_idx);

	void PushDisable();
	void PopDisable();
}

namespace Gui = ImGui;
