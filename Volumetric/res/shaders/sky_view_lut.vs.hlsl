/*==============================================*/
/*  Aster: res/shaders/sky_view_lut.vs.hlsl		*/
/*  Copyright (c) 2020 Anish Bhobe				*/
/*==============================================*/

#include "sky_view_lut.hlsli"

struct VSIn {
	int idx : SV_VERTEXID;
};

struct VSOut {
	float4 position : SV_POSITION;
	float2 uv : UV;
};

VSOut main(VSIn input) {

	float4 vertices[] = {
		float4(-1.0f, -1.0f, 0.0f, 1.0f),
		float4(-1.0f, 3.0f, 0.0f, 1.0f),
		float4(3.0f, -1.0f, 0.0f, 1.0f),
		float4(-1.0f, -1.0f, 0.0f, 1.0f),
	};

	VSOut output;
	output.position = vertices[input.idx];
	output.uv = 0.5f + 0.5f * output.position.xy;

	return output;
}