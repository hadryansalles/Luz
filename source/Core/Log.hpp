#pragma once

#include <spdlog/spdlog.h>

namespace Log {

void Debug(const char* format, ...);
void Info(const char* format, ...);
void Warn(const char* format, ...);
void Error(const char* format, ...);
void Critical(const char* format, ...);

}

namespace spdlog {

class logger;

}

class Logger {
public:
	static void Init();
	static std::shared_ptr<spdlog::logger>& Get();

private:
	static std::shared_ptr<spdlog::logger> _logger;
};

#define LOG_INFO(...)     Logger::Get()->info(__VA_ARGS__)
#define LOG_WARN(...)     Logger::Get()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    Logger::Get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::Get()->critical(__VA_ARGS__); abort();

#ifdef LUZ_DEBUG
#define DEBUG_TRACE(...) Logger::Get()->trace(__VA_ARGS__)
#else
#define DEBUG_TRACE(...)
#endif
