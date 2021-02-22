// =============================================
//  Volumetric: atmosphere_info.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>

struct AtmosphereInfo {
	//ray
	alignas(16) vec3 scatter_coeff_rayleigh; //12
	alignas(04) f32 density_factor_rayleigh; //16

	// ozone
	alignas(16) vec3 absorption_coeff_ozone; //28
	alignas(04) f32 ozone_height;            //32
	alignas(04) f32 ozone_width;             //36

	//mei
	alignas(04) f32 scatter_coeff_mei;    //40
	alignas(04) f32 absorption_coeff_mei; //44
	alignas(04) f32 density_factor_mei;   //48

	alignas(04) f32 asymmetry_mei; //52

	// sampling
	alignas(04) i32 depth_samples; //56
	alignas(04) i32 view_samples;  //60
};
