#pragma once

#include <spdlog/spdlog.h>

class Log {
public:
	static void Init();
	static std::shared_ptr<spdlog::logger>& Get() { return _logger; }

private:
	static std::shared_ptr<spdlog::logger> _logger;
};

#define LOG_INFO(...)        Log::Get()->info(__VA_ARGS__)
#define LOG_WARN(...)        Log::Get()->warn(__VA_ARGS__)
#define LOG_ERROR(...)       Log::Get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...)    Log::Get()->critical(__VA_ARGS__); abort();

#ifdef LUZ_DEBUG
#define DEBUG_TRACE(...) Log::Get()->trace(__VA_ARGS__)
#else
#define DEBUG_TRACE(...)
#endif
