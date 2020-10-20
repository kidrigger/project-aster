/*=========================================*/
/*  Aster: main/files.cc                   */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include "files.h"
#include <EASTL/vector.h>
#include <fstream>

b8 file_exists(const stl::string_view& _name) noexcept {
	struct stat s;
	return stat(_name.data(), &s) == 0;
}

stl::vector<u32> load_binary32_file(const stl::string_view& _name) noexcept {
	ERROR_IF(!file_exists(_name), stl::fmt("File \"%s\" does not exist", _name.data()));

	stl::vector<u32> filedata;
	std::ifstream file(_name.data(), std::ios::ate | std::ios::binary);
	if (file.is_open())
	{
		size_t filesize = file.tellg();
		filedata.resize(filesize / sizeof(u32));
		file.seekg(0);
		file.read((char*)filedata.data(), filesize);
		file.close();
	}
	return filedata;
}