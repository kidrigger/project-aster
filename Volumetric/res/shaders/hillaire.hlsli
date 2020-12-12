/*=========================================*/
/*  Aster: res/shaders/hillaire.hlsli      */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/

#include "structs.hlsli"

[[vk::binding(0, 0)]] cbuffer camera { CameraUbo camera; }
[[vk::binding(1, 0)]] cbuffer sun { SunlightUbo sun; }
[[vk::binding(2, 0)]] cbuffer atmos { AtmosphereParams atmos; }
[[vk::binding(3, 0)]] Texture2D transmittance_lut;
[[vk::binding(3, 0)]] SamplerState lut_sampler;

#include "functions.hlsli"
