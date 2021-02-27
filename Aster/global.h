// =============================================
//  Aster: global.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <config.h>
#include <constants.h>
#include <logger.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <string>

#define VULKAN_HPP_ASSERT(expr) DEBUG_IF(!(expr), "Vulkan assert failed")
#include <vulkan/vulkan.hpp>

#pragma warning(push, 0)
#include <vma/vk_mem_alloc.hpp>
#pragma warning(pop)

#include <ownership.h>
#include <tl/expected.hpp>

#define CODE_LOC " @ " __FILE__ ":" VULKAN_HPP_STRINGIFY(__LINE__)

class Err final : public std::exception {
public:
	explicit Err(const std::string_view& _message, const vk::Result _result) : std::exception{ _message.data() }
	                                                                         , result_{ _result } {}

	explicit Err(const std::string_view& _message, Err&& _next_err) : std::exception{ _message.data() }
	                                                                , result_{ _next_err.result_ }
	                                                                , err_list_{ std::move(_next_err.err_list_) } {
		_next_err.err_list_ = {};
		err_list_.emplace_back(std::forward<Err>(_next_err));
	}

	static tl::unexpected<Err> make(const std::string_view& _msg, const vk::Result _result = vk::Result::eErrorUnknown) {
		return tl::make_unexpected(Err{ _msg, _result });
	}

	static tl::unexpected<Err> make(const std::string_view& _msg, Err&& _next_err) {
		return tl::make_unexpected(Err{ _msg, std::forward<Err>(_next_err) });
	}

	static tl::unexpected<Err> make(Err&& _err) {
		return tl::make_unexpected(_err);
	}

	[[nodiscard]]
	vk::Result code() const {
		return result_;
	}

private:
	vk::Result result_;

	std::vector<Err> err_list_;
};

template <typename Ok = void>
using Res = tl::expected<Ok, Err>;

[[nodiscard]]
inline bool failed(const vk::Result _result) {
	return _result != vk::Result::eSuccess;
}

namespace std {
	namespace impl {
		string format(const char* _fmt, ...);
	}

	template <typename... Ts>
	[[nodiscard]]
	string fmt(const char* _fmt, Ts&&... _args) {
		return impl::format(_fmt, forward<Ts>(_args)...);
	}
}

template <typename T>
concept IsVkEnum = requires(T _t) {
	{ std::is_enum_v<T> };
	{ vk::to_string(_t) } -> std::same_as<std::string>;
};

template <typename T> requires IsVkEnum<T>
[[nodiscard]]
const char* to_cstr(const T& _val) {
	static std::string buffer;
	buffer = vk::to_string(_val);
	return buffer.c_str();
}

// TODO: Check why inline namespaces aren't working in MSVC 19.27.29110
using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

template <typename T> requires vk::isVulkanHandleType<T>::value
[[nodiscard]]
constexpr u64 get_vk_handle(const T& _vk_handle) noexcept {
	return reinterpret_cast<u64>(cast<T::CType>(_vk_handle));
}

template <typename F>
struct std::hash<vk::Flags<F>> {
	[[nodiscard]]
	usize operator()(const vk::Flags<F>& _val) {
		return std::hash<u32>()(cast<u32>(_val));
	}
};

template <typename T>
[[nodiscard]]
usize hash_any(const T& _val) {
	return std::hash<std::remove_cvref_t<T>>()(_val);
}

[[nodiscard]]
inline usize hash_combine(const usize _hash0, const usize _hash1) {
	constexpr usize salt_value = 0x9e3779b9;
	return _hash0 ^ (_hash1 + salt_value + (_hash0 << 6) + (_hash0 >> 2));
}

struct Time {
	static constexpr f64 max_delta = 0.1;

	inline static f64 elapsed{ qnan<f64> };
	inline static f64 delta{ qnan<f64> };

	static void init() {
		WARN_IF(!isnan(elapsed), "Time already init.");
		elapsed = glfwGetTime();
		delta = 1.0 / 60.0;
	}

	static void update() {
		ERROR_IF(isnan(elapsed), "Time not init.");
		const auto new_elapsed = glfwGetTime();
		delta = std::clamp(new_elapsed - elapsed, 0.0, max_delta);
		elapsed = new_elapsed;
	}
};

[[nodiscard]]
inline usize closest_multiple(const usize _val, const usize _of) {
	return _of * ((_val + _of - 1) / _of);
}
