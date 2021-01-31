/*=========================================*/
/*  Aster: res/shaders/hillaire.vs.hlsl    */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/

#include "hillaire.hlsli"

struct VSIn {
	int idx : SV_VERTEXID;
};

struct VSOut {
	float4 position : SV_POSITION;
	float4 ray_direction : RAY_DIRECTION;
	float2 uv : UV;
};

VSOut main(VSIn input) {

	float4 vertices[] = {
		float4(-1.0f, -1.0f, 0.0f, 1.0f),
		float4(-1.0f, 3.0f, 0.0f, 1.0f),
		float4(3.0f, -1.0f, 0.0f, 1.0f),
		float4(-1.0f, -1.0f, 0.0f, 1.0f),
	};

	float dist = 1.0f / tan(camera.fov * 0.5f);
	float3 right = normalize(cross(UP, camera.direction.xyz));
	float3 up = normalize(cross(camera.direction.xyz, right));

	float aspect = camera.screen_size.x * (1.0f / camera.screen_size.y);

	float3 fwd = dist * camera.direction.xyz;

	VSOut output;

	output.position = vertices[input.idx];
	output.ray_direction = float4(fwd + right * aspect * vertices[input.idx].x + up * vertices[input.idx].y, 0.0f);
	output.uv = 0.5f + 0.5f * output.position.xy;

	return output;
}
