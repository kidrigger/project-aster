/*=================================================*/
/*  Aster: res/shaders/transmittance_lut.vs.hlsl   */
/*  Copyright (c) 2020 Anish Bhobe                 */
/*=================================================*/

#include "structs.hlsli"
[[vk::push_constant]] AtmosphereParams atmos;
#include "functions.hlsli"

float optical_length_rayleigh(float2 rmu, float len) {
	float r = rmu.x;
	float mu = rmu.y;
	int lim = atmos.depth_samples;
	float dx = len / float(lim);
	float odepth = 0.0f;
	for (int i = 0; i <= atmos.depth_samples; ++i) {
		float d_i = dx * i;
		float r_i = sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r);
		odepth += density_rayleigh(r_i) * dx * (i == 0 || i == lim ? 0.5f : 1.0f);
	}
	return odepth;
}

float optical_length_mei(float2 rmu, float len) {
	float r = rmu.x;
	float mu = rmu.y;
	int lim = atmos.depth_samples;
	float dx = len / float(lim);
	float odepth = 0.0f;
	for (int i = 0; i <= atmos.depth_samples; ++i) {
		float d_i = dx * i;
		float r_i = sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r);
		odepth += density_mei(r_i) * dx * (i == 0 || i == lim ? 0.5f : 1.0f);
	}
	return odepth;
}

float optical_length_ozone(float2 rmu, float len) {
	float r = rmu.x;
	float mu = rmu.y;
	int lim = atmos.depth_samples;
	float dx = len / float(lim);
	float odepth = 0.0f;
	for (int i = 0; i <= atmos.depth_samples; ++i) {
		float d_i = dx * i;
		float r_i = sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r);
		odepth += density_ozone(r_i) * dx * (i == 0 || i == lim ? 0.5f : 1.0f);
	}
	return odepth;
}

float3 calculate_transmittance(float2 rmu) {
	float len_atm = distance_to_atmosphere(rmu);
	float3 extinction_coeff_mei = atmos.scatter_coeff_mei + atmos.absorption_coeff_mei;

	float3 exp_term = atmos.scatter_coeff_rayleigh * optical_length_rayleigh(rmu, len_atm)
		+ extinction_coeff_mei * optical_length_mei(rmu, len_atm)
		+ atmos.absorption_coeff_ozone * optical_length_ozone(rmu, len_atm);
	return exp(-exp_term);
}


struct FSIn {
	float2 uv : UV;
};

struct FSOut {
	float4 color : SV_TARGET0;
};

FSOut main(FSIn input) {
	float2 rmu = get_rmu_from_uv(input.uv);
	
	FSOut output;
	output.color = float4(calculate_transmittance(rmu), 1.0f);

	return output;
}
