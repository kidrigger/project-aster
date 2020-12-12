/*=========================================*/
/*  Aster: res/shaders/functions.hlsli     */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/

#ifndef _FUNCTIONS_HLSLI
#define _FUNCTIONS_HLSLI

// Util
#include "structs.hlsli"

bool isnan(float a) {
	return (a > 0 || a < 0 || a == 0) ? false : true;
}

bool isnan(float2 a) {
	return isnan(a.x) || isnan(a.y);
}

bool isnan(float3 a) {
	return isnan(a.x) || isnan(a.y) || isnan(a.z);
}

bool isinf(float a) {
	return a == a + 1;
}

bool isinf(float2 a) {
	return isinf(a.x) || isinf(a.y);
}

bool isinf(float3 a) {
	return isinf(a.x) || isinf(a.y) || isinf(a.z);
}

static const float3 UP = float3(0, 1, 0);
static const float3 RIGHT = float3(1, 0, 0);
static const float3 FWD = float3(0, 0, 1);

static const float3 MAGENTA = float3(1, 0, 1);
static const float3 RED = float3(1, 0, 0);
static const float3 GREEN = float3(0, 1, 0);
static const float3 BLUE = float3(0, 0, 1);

float3 marknan(float3 a, float3 col) {
	return isnan(a) ? col : a;
}

float3 markinf(float3 a, float3 col) {
	return isinf(a) ? col : a;
}

// Impl

static const float Rg = 6360000.0f;
static const float Ra = 6460000.0f;
static const float PI = 3.14159265f;
static const float PI_INV = 0.3183098862f;

static const int TRANSMITTANCE_TEXTURE_WIDTH = 64;
static const int TRANSMITTANCE_TEXTURE_HEIGHT = 256;

float get_r(float3 x) {
	return length(x);
}

float get_mu(float3 x, float3 v) {
	return dot(x, v) / length(x);
}

float2 get_rmu(float3 x, float3 v) {
	return float2(get_r(x), get_mu(x, v));
}

bool in_atmosphere(float r) {
	return r <= Ra;
}

bool in_ground(float r) {
	return r <= Rg;
}

float distance_to_atmosphere_ext(float2 rmu) {
	float b = rmu.x * rmu.y;
	float c = rmu.x * rmu.x - Ra * Ra;
	float disc = b * b - c;

	if (disc < 0.0f) {
		return 1.#INF;
	}
	else {
		float delta = sqrt(disc);
		float t = -b - delta;

		return t >= 0.0f ? t : 1.#INF;
	}
}

float distance_to_atmosphere_ext(float3 x, float3 v) {
	return distance_to_atmosphere_ext(get_rmu(x, v));
}

float distance_to_atmosphere(float2 rmu) {
	float b = rmu.x * rmu.y;
	float c = rmu.x * rmu.x - Ra * Ra;
	float delta = sqrt(b * b - c);
	return -b + delta;
}

float distance_to_atmosphere(float3 x, float3 v) {
	return distance_to_atmosphere(get_rmu(x, v));
}

float distance_to_ground(float2 rmu) {
	float b = rmu.x * rmu.y;
	float c = rmu.x * rmu.x - Rg * Rg;
	float disc = b * b - c;

	if (disc < 0.0f) {
		return 1.#INF;
	}
	else {
		float delta = sqrt(disc);
		float t = -b - delta;

		return t >= 0.0f ? t : 1.#INF;
	}
}

float distance_to_ground(float3 x, float3 v) {
	return distance_to_ground(get_rmu(x, v));
}

float Pr(float nu) {
	float k = 3.0f * PI_INV / 16.0f;
	return k * (1 + nu * nu);
}

float Pm(float nu) {
	float k = 3.0f * PI_INV / 8.0f;
	float g = atmos.assymetry_mei;
	float g2 = g * g;
	float factor = 1 + g2 - 2.0f * g * nu;
	return (1 - g2) * (1 + nu * nu) / ((2 + g2) * pow(max(0.0000001f, factor), 1.5f));
}

float density_rayleigh(float r) {
	float h = r - Rg;
	return exp(-h / atmos.density_factor_rayleigh);
}

float density_mei(float r) {
	float h = r - Rg;
	return exp(-h / atmos.density_factor_mei);
}

float density_ozone(float r) {
	float h = r - Rg;
	return max(0, 1 - abs(h - atmos.ozone_height) * 2.0f / atmos.ozone_width);
}

float get_tex_coord_from_unit_range(float x, int texture_size) {
	return 0.5 / float(texture_size) + x * (1.0 - 1.0 / float(texture_size));
}

float get_unit_range_from_tex_coord(float u, int texture_size) {
	return (u - 0.5 / float(texture_size)) / (1.0 - 1.0 / float(texture_size));
}

float2 get_rmu_from_uv(float2 uv) {
	float x_r = get_unit_range_from_tex_coord(uv.x, TRANSMITTANCE_TEXTURE_WIDTH);
	float x_mu = get_unit_range_from_tex_coord(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT);
	// Distance to top atmosphere boundary for a horizontal ray at ground level.
	float h = sqrt(Ra * Ra - Rg * Rg);
	// Distance to the horizon, from which we can compute r:
	float rho = h * x_r;
	float r = sqrt(rho * rho + Rg * Rg);
	// Distance to the top atmosphere boundary for the ray (r,mu), and its minimum
	// and maximum values over all mu - obtained for (r,1) and (r,mu_horizon) -
	// from which we can recover mu:
	float d_min = Ra - r;
	float d_max = rho + h;
	float d = d_min + x_mu * (d_max - d_min);
	float mu = d == 0.0 ? float(1.0) : (h * h - rho * rho - d * d) / (2.0 * r * d);
	mu = clamp(mu, -1.0f, 1.0f);

	return float2(r, mu);
}

float2 get_uv_from_rmu(float2 rmu) {
	// Distance to top atmosphere boundary for a horizontal ray at ground level.
	float h = sqrt(Ra * Ra - Rg * Rg);
	// Distance to the horizon.
	float rho = sqrt(max(0, rmu.x * rmu.x - Rg * Rg));
	float d = distance_to_atmosphere(rmu);
	float d_min = Ra - rmu.x;
	float d_max = rho + h;
	float x_mu = (d - d_min) / (d_max - d_min);
	float x_r = rho / h;
	return float2(get_tex_coord_from_unit_range(x_r, TRANSMITTANCE_TEXTURE_WIDTH),
		get_tex_coord_from_unit_range(x_mu, TRANSMITTANCE_TEXTURE_HEIGHT));
}

#endif // _FUNCTIONS_HLSLI
