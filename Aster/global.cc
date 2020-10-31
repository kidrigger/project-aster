/*=========================================*/
/*  Aster: global.cc	                   */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#include "global.h"

#include <EASTL/allocator.h>
#include <EASTL/string.h>
#include <cstdio>

#pragma warning(push, 0)
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#pragma warning(pop)

// NOTE: Vulkan Dispatch Loader Storage - Should only appear once.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

Time g_time = Time();

namespace eastl {
	/* Copied From: https://gist.github.com/Const-me/8c106cdd968587b11de4212b6e84573a */
	/* Credits to Const-me */
	// Implement allocators.
	// The default ones are incompatible with Windows because they use global operator delete, which is incompatible with memory allocated by _aligned_malloc API.
	// Because of this issue, you must define EASTL_USER_DEFINED_ALLOCATOR everywhere you use any EASTL stuff.
	// For example, you could add that define in project setting (for every combination of platform + configuration), or in stdafx.h precompiled header.
	allocator::allocator(const char* EASTL_NAME(pName)) {
#ifndef EASTL_USER_DEFINED_ALLOCATOR
		static_assert(false, "You must define EASTL_USER_DEFINED_ALLOCATOR on Windows.");
#endif

#if EASTL_NAME_ENABLED
		mpName = pName ? pName : EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
	}

	allocator::allocator(const allocator& EASTL_NAME(alloc)) {
#if EASTL_NAME_ENABLED
		mpName = alloc.mpName;
#endif
	}

	allocator::allocator(const allocator&, const char* EASTL_NAME(pName)) {
#if EASTL_NAME_ENABLED
		mpName = pName ? pName : EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
	}

	allocator& allocator::operator=(const allocator& EASTL_NAME(alloc)) {
#if EASTL_NAME_ENABLED
		mpName = alloc.mpName;
#endif
		return *this;
	}

	const char* allocator::get_name() const {
#if EASTL_NAME_ENABLED
		return mpName;
#else
		return EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
	}

	void allocator::set_name(const char* EASTL_NAME(pName)) {
#if EASTL_NAME_ENABLED
		mpName = pName;
#endif
	}

	void* allocator::allocate(size_t n, int /* flags */) {
		// Using aligned version of malloc to be able to use _aligned_free everywhere.
		// Apparently, EASTL doesn't distinguish aligned versus unaligned memory, this means we must use _aligned_malloc / _aligned_free everywhere or it will prolly crash in runtime.
		constexpr size_t defaultAlignment = alignof(void*);

		return _aligned_malloc(n, defaultAlignment);
	}

	void* allocator::allocate(size_t n, size_t alignment, size_t offset, int /* flags */) {
		return  _aligned_offset_malloc(n, alignment, offset);
	}

	void allocator::deallocate(void* p, size_t) {
		return _aligned_free(p);
	}

	bool operator==(const allocator&, const allocator&) {
		return true; // All allocators are considered equal, as they merely use global new/delete.
	}

	bool operator!=(const allocator&, const allocator&) {
		return false; // All allocators are considered equal, as they merely use global new/delete.
	}
}

int Vsnprintf8(char* pDestination, size_t n, const char* pFormat, va_list arguments) {
	//ERROR_IF(pDestination == nullptr, "Null buffer") THEN_CRASH(1) ELSE_IF_ERROR(n == 0, "Empty buffer") THEN_CRASH(1);
#ifdef _MSC_VER
	auto v = vsnprintf(pDestination, n, pFormat, arguments);
	ERROR_IF(v == 0, "Final requirement cannot be 0") THEN_CRASH(1);
	return v;
#else
	return vsnprintf(pDestination, n, pFormat, arguments);
#endif
}

int VsnprintfW(wchar_t* pDestination, size_t n, const wchar_t* pFormat, va_list arguments) {
	//ERROR_IF(pDestination == nullptr, "Null buffer") THEN_CRASH(1) ELSE_IF_ERROR(n == 0, "Empty buffer") THEN_CRASH(1);
#ifdef _MSC_VER
	if (pDestination == nullptr && n == 0) {
		return _vscwprintf(pFormat, arguments);
	} else {
		return _vsnwprintf_s(pDestination, n, _TRUNCATE, pFormat, arguments);
	}
#else
	char* d = new char[n + 1];
	int r = vsnprintf(d, n, convertstring<char16_t, char>(pFormat).c_str(), arguments);
	memcpy(pDestination, convertstring<char, char16_t>(d).c_str(), (n + 1) * sizeof(char16_t));
	delete[] d;
	return r;
#endif
}

int Vsnprintf16(char16_t* pDestination, size_t n, const char16_t* pFormat, va_list arguments) {
	//ERROR_IF(pDestination == nullptr, "Null buffer") THEN_CRASH(1) ELSE_IF_ERROR(n == 0, "Empty buffer") THEN_CRASH(1);
#ifdef _MSC_VER
	if (pDestination == nullptr && n == 0) {
		return _vscwprintf((wchar_t*)pFormat, arguments);
	} else {
		return _vsnwprintf_s((wchar_t*)pDestination, n, _TRUNCATE, (wchar_t*)pFormat, arguments);
	}
#else
	char* d = new char[n + 1];
	int r = vsnprintf(d, n, convertstring<char16_t, char>(pFormat).c_str(), arguments);
	memcpy(pDestination, convertstring<char, char16_t>(d).c_str(), (n + 1) * sizeof(char16_t));
	delete[] d;
	return r;
#endif
}
