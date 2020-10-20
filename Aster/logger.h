/*=========================================*/
/*  Aster: logger.h                        */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <config.h>
#include <constants.h>
#include <string>
#include <EASTL/string.h>

struct Logger {
	enum class LogType : u32 {
		eError,
		eWarning,
		eInfo,
		eDebug,
		eVerbose,
	};

	u32 minimum_logging_level{ cast<u32>(LogType::eDebug) };

	inline void set_minimum_logging_level(LogType _log_type) { minimum_logging_level = cast<u32>(_log_type); }

	template <LogType log_t>
	inline constexpr static const char* to_cstr() {
		if constexpr (log_t == LogType::eError) return "[ERROR]:";
		if constexpr (log_t == LogType::eWarning) return "[WARN]: ";
		if constexpr (log_t == LogType::eInfo) return "[INFO]: ";
		if constexpr (log_t == LogType::eDebug) return "[DEBUG]:";
		if constexpr (log_t == LogType::eVerbose) return "[VERB]: ";
	}

	template <LogType log_t>
	inline constexpr static const char* to_color_cstr() {
		if constexpr (log_t == LogType::eError) return ANSI_Red;
		if constexpr (log_t == LogType::eWarning) return ANSI_Yellow;
		if constexpr (log_t == LogType::eInfo) return ANSI_Green;
		if constexpr (log_t == LogType::eDebug) return ANSI_White;
		if constexpr (log_t == LogType::eVerbose) return ANSI_Blue;
	}

	template <LogType log_t>
	inline void log(const char* _message, const char* loc, u32 line) {
		if (cast<u32>(log_t) <= minimum_logging_level)
			printf("%s%s %s%s| at %s:%u%s\n", to_color_cstr<log_t>(), to_cstr<log_t>(), _message, ANSI_Black, loc, line, ANSI_Reset);
#if !defined(NDEBUG)
		if constexpr (log_t == LogType::eError) {
			__debugbreak();
		}
#endif // !defined(NDEBUG)
	}

    template <LogType log_t>
    inline void log(const eastl::string& message, const char* loc, u32 line) {
        return log<log_t>(message.data(), loc, line);
	}

	template <LogType log_t>
	[[deprecated]] inline void log(const std::string& message, const char* loc, u32 line) {
		return log<log_t>(message.c_str(), loc, line);
	}

	template <LogType log_t>
	inline void log_cond(const char* expr_str, const char* message, const char* loc, u32 line) {
		if (cast<u32>(log_t) <= minimum_logging_level)
			printf("%s%s (%s) %s%s| at %s:%u%s\n", to_color_cstr<log_t>(), to_cstr<log_t>(), expr_str, message, ANSI_Black, loc, line, ANSI_Reset);
#if !defined(NDEBUG)
		if constexpr (log_t == LogType::eError) {
			__debugbreak();
		}
#endif // !defined(NDEBUG)
	}

    template <LogType log_t>
    inline void log_cond(const char* expr_str, const eastl::string& message, const char* loc, u32 line) {
        log_cond<log_t>(expr_str, message.data(), loc, line);
	}

	template <LogType log_t>
	[[deprecated]] inline void log_cond(const char* expr_str, const std::string& message, const char* loc, u32 line) {
		log_cond<log_t>(expr_str, message.c_str(), loc, line);
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

#define CRASH(code) exit(cast<i32>(code))
#define THEN_CRASH(code) ,CRASH(code)
