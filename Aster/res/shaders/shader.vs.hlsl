// =============================================
//  Aster: shader.vs.hlsl
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "shader.hlsli"

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

	output.position = mul(camera.projection, mul(camera.view, input.position));
	output.color = input.color;
	return output;
}
