﻿// =============================================
//  Volumetric: sun_data.h
//  Copyright (c) 2020-2022 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>

struct SunData {
	alignas(16) vec3 direction;
	alignas(04) i32 pad0;
	alignas(16) vec3 intensities;
	alignas(04) i32 pad1;
};
