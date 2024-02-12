#include "Log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

std::shared_ptr<spdlog::logger> Logger::_logger;
	
void Logger::Init() {
	std::vector<spdlog::sink_ptr> sinks;

	sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Luz.log", true));

	sinks[0]->set_pattern("%^[%T] %v%$");
	sinks[1]->set_pattern("[%T] [%l] %v");

	_logger = std::make_shared<spdlog::logger>("Luz", sinks.begin(), sinks.end());
	spdlog::register_logger(_logger);
	_logger->set_level(spdlog::level::trace);
	_logger->flush_on(spdlog::level::trace);
}

std::shared_ptr<spdlog::logger>& Logger::Get() {
    return _logger;
}

namespace Log {

void Debug(const char* format, ...) {
    va_list vaArgs;
    va_start(vaArgs, format);

    va_list vaArgsCopy;
    va_copy(vaArgsCopy, vaArgs);
    const int length = std::vsnprintf(NULL, 0, format, vaArgsCopy);
    va_end(vaArgsCopy);

    std::vector<char> zc(length + 1);
    std::vsnprintf(zc.data(), zc.size(), format, vaArgs);
    va_end(vaArgs);

    std::string s(zc.data(), length);
    DEBUG_TRACE(s.c_str());
}

void Info(const char* format, ...) {
    va_list vaArgs;
    va_start(vaArgs, format);

    va_list vaArgsCopy;
    va_copy(vaArgsCopy, vaArgs);
    const int length = std::vsnprintf(NULL, 0, format, vaArgsCopy);
    va_end(vaArgsCopy);

    std::vector<char> zc(length + 1);
    std::vsnprintf(zc.data(), zc.size(), format, vaArgs);
    va_end(vaArgs);

    std::string s(zc.data(), length);
    LOG_INFO(s.c_str());
}

void Warn(const char* format, ...) {
    va_list vaArgs;
    va_start(vaArgs, format);

    va_list vaArgsCopy;
    va_copy(vaArgsCopy, vaArgs);
    const int length = std::vsnprintf(NULL, 0, format, vaArgsCopy);
    va_end(vaArgsCopy);

    std::vector<char> zc(length + 1);
    std::vsnprintf(zc.data(), zc.size(), format, vaArgs);
    va_end(vaArgs);

    std::string s(zc.data(), length);
    LOG_WARN(s.c_str());
}

void Error(const char* format, ...) {
    va_list vaArgs;
    va_start(vaArgs, format);

    va_list vaArgsCopy;
    va_copy(vaArgsCopy, vaArgs);
    const int length = std::vsnprintf(NULL, 0, format, vaArgsCopy);
    va_end(vaArgsCopy);

    std::vector<char> zc(length + 1);
    std::vsnprintf(zc.data(), zc.size(), format, vaArgs);
    va_end(vaArgs);

    std::string s(zc.data(), length);
    LOG_ERROR(s.c_str());
}

void Critical(const char* format, ...) {
    va_list vaArgs;
    va_start(vaArgs, format);

    va_list vaArgsCopy;
    va_copy(vaArgsCopy, vaArgs);
    const int length = std::vsnprintf(NULL, 0, format, vaArgsCopy);
    va_end(vaArgsCopy);

    std::vector<char> zc(length + 1);
    std::vsnprintf(zc.data(), zc.size(), format, vaArgs);
    va_end(vaArgs);

    std::string s(zc.data(), length);
    LOG_CRITICAL(s.c_str());
}

}
