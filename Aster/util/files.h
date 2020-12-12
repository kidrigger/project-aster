/*=========================================*/
/*  Aster: core/files.h                    */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>
#include <vector>
#include <fstream>

b8 file_exists(const stl::string_view& _name) noexcept;
stl::vector<u32> load_binary32_file(const stl::string_view& _name) noexcept;