// =============================================
//  Aster: files.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <global.h>
#include <vector>

b8 file_exists(const std::string_view& _name) noexcept;
std::vector<u32> load_binary32_file(const std::string_view& _name) noexcept;
