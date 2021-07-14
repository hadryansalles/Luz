#include "Log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

std::shared_ptr<spdlog::logger> Log::_logger;
	
void Log::Init() {
	std::vector<spdlog::sink_ptr> sinks;

	sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("Luz.log", true));

	sinks[0]->set_pattern("%^[%T] %n: %v%$");
	sinks[1]->set_pattern("[%T] [%l] %n: %v");

	_logger = std::make_shared<spdlog::logger>("Luz", sinks.begin(), sinks.end());
	spdlog::register_logger(_logger);
	_logger->set_level(spdlog::level::trace);
	_logger->flush_on(spdlog::level::trace);
}
