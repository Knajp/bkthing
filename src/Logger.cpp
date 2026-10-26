#include "Logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <memory>
#include <vector>
#include <mutex>

namespace game
{
void Logger::init()
{
  static std::once_flag initialized;

  std::call_once(initialized, []()
                 {
                  std::vector<spdlog::sink_ptr> sinks;
                  auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

                  auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/sor.log", 5 * 1024 * 1024, 3);
                  
                  sinks.push_back(consoleSink);
                  sinks.push_back(fileSink);
                  
                  auto logger = std::make_shared<spdlog::logger>("Game", sinks.begin(), sinks.end());

                  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");

                  logger->set_level(spdlog::level::debug);
                  logger->flush_on(spdlog::level::warn);

                  spdlog::set_default_logger(logger);
                  
                 });
}

void Logger::shutdown()
{
  spdlog::shutdown();
}
}
