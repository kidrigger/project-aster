/*=========================================*/
/*  Aster: res/shader.fs.hlsl              */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/

#include "shader.hlsli"

float4 main(float4 color : COLOR_ATTR) : SV_TARGET {
	return color;
}
