// =============================================
//  Aster: global.cc
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#include "global.h"

#include <cstdio>
#include <cstdarg>

#pragma warning(push, 0)
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#pragma warning(pop)

// NOTE: Vulkan Dispatch Loader Storage - Should only appear once.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

Time g_time = Time();

//int Vsnprintf8(char* pDestination, size_t n, const char* pFormat, va_list arguments) {
//	//ERROR_IF(pDestination == nullptr, "Null buffer") THEN_CRASH(1) ELSE_IF_ERROR(n == 0, "Empty buffer") THEN_CRASH(1);
//#ifdef _MSC_VER
//	auto v = vsnprintf(pDestination, n, pFormat, arguments);
//	ERROR_IF(v == 0, "Final requirement cannot be 0") THEN_CRASH(1);
//	return v;
//#else
//	return vsnprintf(pDestination, n, pFormat, arguments);
//#endif
//}
//
//int VsnprintfW(wchar_t* pDestination, size_t n, const wchar_t* pFormat, va_list arguments) {
//	//ERROR_IF(pDestination == nullptr, "Null buffer") THEN_CRASH(1) ELSE_IF_ERROR(n == 0, "Empty buffer") THEN_CRASH(1);
//#ifdef _MSC_VER
//	if (pDestination == nullptr && n == 0) {
//		return _vscwprintf(pFormat, arguments);
//	} else {
//		return _vsnwprintf_s(pDestination, n, _TRUNCATE, pFormat, arguments);
//	}
//#else
//	char* d = new char[n + 1];
//	int r = vsnprintf(d, n, convertstring<char16_t, char>(pFormat).c_str(), arguments);
//	memcpy(pDestination, convertstring<char, char16_t>(d).c_str(), (n + 1) * sizeof(char16_t));
//	delete[] d;
//	return r;
//#endif
//}
//
//int Vsnprintf16(char16_t* pDestination, size_t n, const char16_t* pFormat, va_list arguments) {
//	//ERROR_IF(pDestination == nullptr, "Null buffer") THEN_CRASH(1) ELSE_IF_ERROR(n == 0, "Empty buffer") THEN_CRASH(1);
//#ifdef _MSC_VER
//	if (pDestination == nullptr && n == 0) {
//		return _vscwprintf((wchar_t*)pFormat, arguments);
//	} else {
//		return _vsnwprintf_s((wchar_t*)pDestination, n, _TRUNCATE, (wchar_t*)pFormat, arguments);
//	}
//#else
//	char* d = new char[n + 1];
//	int r = vsnprintf(d, n, convertstring<char16_t, char>(pFormat).c_str(), arguments);
//	memcpy(pDestination, convertstring<char, char16_t>(d).c_str(), (n + 1) * sizeof(char16_t));
//	delete[] d;
//	return r;
//#endif
//}

std::string std::impl::format(const char* _fmt, ...) {
	va_list args;
	va_start(args, _fmt);

	const auto req = vsnprintf(nullptr, 0, _fmt, args) + 1;
	string buf(req, '\0');
	vsnprintf(buf.data(), buf.size(), _fmt, args);

	va_end(args);
	return buf;
}
