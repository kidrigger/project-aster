// =============================================
//  Volumetric: sky_view_lut.fs.hlsl
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#include "sky_view_lut.hlsli"

struct FSIn {
	float2 uv : UV;
};

struct FSOut {
	float4 color : SV_TARGET0;
};

// Transmittance
float3 T(float3 x, float3 y) {
	float len = distance(x, y);
	if (len == 0.0f) return 1.0f;
	float3 v = (y - x) / len;
	float2 rmu_x = get_rmu(x, v);
	float2 rmu_y = get_rmu(y, v);
	float2 xuv = get_transmittance_uv_from_rmu(rmu_x);
	float2 yuv = get_transmittance_uv_from_rmu(rmu_y);
	return transmittance_lut.Sample(lut_sampler, xuv).rgb / max(1e-7f, transmittance_lut.Sample(lut_sampler, yuv).rgb);
}

float Vis(float2 rmu) {
	float dg = distance_to_ground(rmu);
	return step(1.#INF, dg);
}

float3 S(float3 x, float3 v) {
	float2 rmu = get_rmu(x, v);
	float t_atm = distance_to_atmosphere(rmu);
	return Vis(rmu) * T(x, x + t_atm * v);
}

float3 L_scat(float3 c, float3 x, float3 v) {
	float3 li = -normalize(sun.direction);
	float r = get_r(x);
	float nu = dot(v, li);
	float3 rayleigh_factor = Pr(nu) * atmosphere.scatter_coeff_rayleigh * density_rayleigh(r);
	float mei_factor = Pm(nu) * atmosphere.scatter_coeff_mei * density_mei(r);
	return T(c, x) * S(x, li) * (rayleigh_factor + mei_factor) * sun.intensities;
}

float3 L(float3 c, float3 v) {
	v = normalize(v);
	float alen = distance_to_atmosphere(c, v);
	float glen = distance_to_ground(c, v);
	float len = min(alen, glen);
	int lim = atmosphere.view_samples;
	float dt = len / float(lim);

	bool ground = !isinf(glen);

	float3 acc = 0.0f;

	if (!ground) {
		for (int i = 0; i <= lim; ++i) {
			float t = i * dt;
			acc += L_scat(c, c + t * v, v) * dt * (i == 0 || i == lim ? 0.5f : 1.0f);
		}
	}
	else {
		for (int i = 0; i <= lim; ++i) {
			float t = i * dt;
			acc += L_scat(c + t * v, c, v) * dt * (i == 0 || i == lim ? 0.5f : 1.0f);
		}
	}

	acc += step(glen, 0) * T(c + len * v, c) * -normalize(sun.direction).y * 0.3f * sun.intensities;

	return acc;
}

float3 get_skyview(float2 longlat) {
	float3 dir = get_skyview_dir_from_longlat(longlat);
	float3 x = float3(0, camera.position.y + Rg, 0);
	return L(x, dir);
}

FSOut main(FSIn input) {

	FSOut output;

	const float2 longlat = get_skyview_longlat_from_uv(input.uv);
	output.color = float4(get_skyview(longlat), 1.0f);
	output.color = output.color / (1.0f + output.color);

	return output;
}