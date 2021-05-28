/*=========================================*/
/*  Aster: res/shaders/hillaire.fs.hlsl    */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/

#include "hillaire.hlsli"

struct FSIn {
	float4 dir : RAY_DIRECTION;
	float2 uv : UV;
};

struct FSOut {
	float4 light : SV_TARGET0;
};

FSOut main(FSIn input) {
	//float3 x = float3(0, camera.position.y + Rg, 0);
	float3 v = normalize(input.dir.xyz);

	FSOut output;

	//output.light = float4(L(x, v), 1.0f);
	//output.light = output.light / (1.0f + output.light);

	const float2 skyview_uv = get_skyview_uv_from_dir(v);
	
	output.light = skyview_lut.Sample(lut_sampler, skyview_uv);
	//output.light = float4(get_skyview_dir_from_longlat(get_skyview_longlat_from_dir(v)), 1.0f);
	output.light = output.light / (1.0f + output.light);
	
	return output;
}
