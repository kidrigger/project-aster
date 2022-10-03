// =============================================
//  Volumetric: structs.hlsli
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#ifndef _STRUCTS_HLSLI
#define _STRUCTS_HLSLI

struct AtmosphereParams {
	//ray
	float3 scatter_coeff_rayleigh;	//12
	float density_factor_rayleigh;	//16

	// ozone
	float3 absorption_coeff_ozone;	//28
	float ozone_height;				//32
	float ozone_width;				//36

	//mei
	float scatter_coeff_mei;		//40
	float absorption_coeff_mei;		//44
	float density_factor_mei;		//48

	float asymmetry_mei;			//52

	// sampling
	int depth_samples;				//56
	int view_samples;				//60
};

struct CameraUbo {
	float4x4 projection;
	float4x4 view;
	float3   position;
	float    near_plane;
	float3   direction;
	float    far_plane;
	float2   screen_size;
	float	 fov;
	int		 pad_;
};

struct SunlightUbo {
	float3 direction;
	int view_samples;
	float3 intensities;
	float _pad1;
};

#endif // _STRUCTS_HLSLI
