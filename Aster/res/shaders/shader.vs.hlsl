/*=========================================*/
/*  Aster: res/shader.vs.hlsl              */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/

#include "shader.hlsli"

struct CameraUbo {
	float4x4 model;
};

cbuffer camera : register(b0, space0) { CameraUbo camera; }

struct VSInput {
	float4 position : POSITION;
	float4 color : COLOR;
};

struct VSOut {
	float4 position : SV_POSITION;
	float4 color : COLOR_ATTR;
};

VSOut main(VSInput input) {
	VSOut output;

	output.position = mul(input.position, camera.model) + float4(0.0f, 0.0f, 0.5f, 0.0f);
	output.color = input.color;
	return output;
}
