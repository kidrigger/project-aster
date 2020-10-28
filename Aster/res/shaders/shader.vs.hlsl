/*=========================================*/
/*  Aster: res/shader.vs.hlsl              */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/

float4 main(float4 pos : POSITION, float4 color : COLOR, out float4 o_color : COLOR_ATTR) : SV_POSITION
{
	o_color = color;
	return pos;
}
