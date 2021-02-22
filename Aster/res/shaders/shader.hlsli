// =============================================
//  Aster: shader.hlsli
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

struct CameraUbo {
	float4x4 projection;
	float4x4 view;
	float3 position;
	float near_plane;
	float3 direction;
	float far_plane;
	float2 screen_size;
	float fov;
};

cbuffer camera : register(b0, space0) { CameraUbo camera; }
