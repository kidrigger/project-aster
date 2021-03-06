// =============================================
//  Aster: logger.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <constants.h>
#include <string>

struct Logger {
	enum class LogType : u32 {
		eError,
		eWarning,
		eInfo,
		eDebug,
		eVerbose,
	};

	u32 minimum_logging_level{ cast<u32>(LogType::eDebug) };

	void set_minimum_logging_level(LogType _log_type) {
		minimum_logging_level = cast<u32>(_log_type);
	}

	template <LogType LogLevel>
	constexpr static const char* to_cstr() {
		if constexpr (LogLevel == LogType::eError) return "[ERROR]:";
		if constexpr (LogLevel == LogType::eWarning) return "[WARN]: ";
		if constexpr (LogLevel == LogType::eInfo) return "[INFO]: ";
		if constexpr (LogLevel == LogType::eDebug) return "[DEBUG]:";
		if constexpr (LogLevel == LogType::eVerbose) return "[VERB]: ";
	}

	template <LogType LogLevel>
	constexpr static const char* to_color_cstr() {
		if constexpr (LogLevel == LogType::eError) return ANSI_Red;
		if constexpr (LogLevel == LogType::eWarning) return ANSI_Yellow;
		if constexpr (LogLevel == LogType::eInfo) return ANSI_Green;
		if constexpr (LogLevel == LogType::eDebug) return ANSI_White;
		if constexpr (LogLevel == LogType::eVerbose) return ANSI_Blue;
	}

	template <LogType LogLevel>
	void log(const std::string_view& _message, const char* _loc, u32 _line) const {
		if (cast<u32>(LogLevel) <= minimum_logging_level) {
			printf("%s%s %s%s| at %s:%u%s\n", to_color_cstr<LogLevel>(), to_cstr<LogLevel>(), _message.data(), ANSI_Black, _loc, _line, ANSI_Reset);
		}
#if !defined(NDEBUG)
		if constexpr (LogLevel == LogType::eError) {
			__debugbreak();
		}
#endif // !defined(NDEBUG)
	}

	template <LogType LogLevel>
	void log_cond(const char* _expr_str, const std::string_view& _message, const char* _loc, u32 _line) const {
		if (cast<u32>(LogLevel) <= minimum_logging_level) {
			printf("%s%s (%s) %s%s| at %s:%u%s\n", to_color_cstr<LogLevel>(), to_cstr<LogLevel>(), _expr_str, _message.data(), ANSI_Black, _loc, _line, ANSI_Reset);
		}
#if !defined(NDEBUG)
		if constexpr (LogLevel == LogType::eError) {
			__debugbreak();
		}
#endif // !defined(NDEBUG)
	}
};

extern Logger g_logger;

#define ERROR(msg) g_logger.log<Logger::LogType::eError>(msg, __FILE__, __LINE__)
#define WARN(msg) g_logger.log<Logger::LogType::eWarning>(msg, __FILE__, __LINE__)
#define INFO(msg) g_logger.log<Logger::LogType::eInfo>(msg, __FILE__, __LINE__)

#define ERROR_IF(expr, msg) if (cast<bool>(expr)) [[unlikely]] g_logger.log_cond<Logger::LogType::eError>(#expr, msg, __FILE__, __LINE__)
#define WARN_IF(expr, msg) if (cast<bool>(expr)) [[unlikely]] g_logger.log_cond<Logger::LogType::eWarning>(#expr, msg, __FILE__, __LINE__)
#define	INFO_IF(expr, msg) if (cast<bool>(expr)) g_logger.log_cond<Logger::LogType::eInfo>(#expr, msg, __FILE__, __LINE__)

#define ELSE_IF_ERROR(expr, msg) ;else if (cast<bool>(expr)) [[unlikely]] g_logger.log_cond<Logger::LogType::eError>(#expr, msg, __FILE__, __LINE__)
#define ELSE_IF_WARN(expr, msg) ;else if (cast<bool>(expr)) [[unlikely]] g_logger.log_cond<Logger::LogType::eWarning>(#expr, msg, __FILE__, __LINE__)
#define	ELSE_IF_INFO(expr, msg) ;else if (cast<bool>(expr)) g_logger.log_cond<Logger::LogType::eInfo>(#expr, msg, __FILE__, __LINE__)

#define ELSE_ERROR(msg) ;else [[unlikely]] g_logger.log<Logger::LogType::eError>(msg, __FILE__, __LINE__)
#define ELSE_WARN(msg) ;else [[unlikely]] g_logger.log<Logger::LogType::eWarning>(msg, __FILE__, __LINE__)
#define ELSE_INFO(msg) ;else g_logger.log<Logger::LogType::eInfo>(msg, __FILE__, __LINE__)

#if !defined(DEBUG_LOG_DISABLED) && !defined(NDEBUG)

#define DEBUG(msg) g_logger.log<Logger::LogType::eDebug>(msg, __FILE__, __LINE__)
#define DEBUG_IF(expr, msg) if (cast<bool>(expr)) g_logger.log_cond<Logger::LogType::eDebug>(#expr, msg, __FILE__, __LINE__)
#define ELSE_IF_DEBUG(expr, msg) ;else if (cast<bool>(expr)) g_logger.log_cond<Logger::LogType::eDebug>(#expr, msg, __FILE__, __LINE__)
#define ELSE_DEBUG(msg) ;else [[unlikely]] g_logger.log<Logger::LogType::eDebug>(msg, __FILE__, __LINE__)

#else // !defined(DEBUG_LOG_DISABLED)

#define DEBUG(msg) {}
#define DEBUG_IF(expr, msg) if (expr) (void)msg
#define ELSE_IF_DEBUG(expr, msg) ;if (expr) (void)msg
#define ELSE_DEBUG(msg) ;{}

#endif // !defined(DEBUG_LOG_DISABLED)

#if !defined(VERBOSE_LOG_DISABLED) && !defined(NDEBUG)

#define VERBOSE(msg) g_logger.log<Logger::LogType::eVerbose>(msg, __FILE__, __LINE__)
#define VERBOSE_IF(expr, msg) if (cast<bool>(expr)) g_logger.log_cond<Logger::LogType::eVerbose>(#expr, msg, __FILE__, __LINE__)
#define ELSE_IF_VERBOSE(expr, msg) ;else if (cast<bool>(expr)) g_logger.log_cond<Logger::LogType::eVerbose>(#expr, msg, __FILE__, __LINE__)
#define ELSE_VERBOSE(msg) ;else g_logger.log<Logger::LogType::eVerbose>(msg, __FILE__, __LINE__)

#else // !defined(DEBUG_LOG_DISABLED)

#define VERBOSE(msg) {}
#define VERBOSE_IF(expr, msg) if (expr) (void)msg
#define ELSE_IF_VERBOSE(expr, msg) ;if (expr) (void)msg
#define ELSE_VERBOSE(msg) ;{}

#endif // !defined(VERBOSE_LOG_DISABLED)

#define DO(code) , code
#define CRASH(code) exit(cast<i32>(code))
#define THEN_CRASH(code) ,CRASH(code)
