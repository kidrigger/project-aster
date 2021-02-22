// =============================================
//  Aster: config.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#define GLFW_INCLUDE_VULKAN
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS

#if defined(NDEBUG)
#define USE_OPTICK (0)
#else
#define USE_OPTICK (1)
#endif
