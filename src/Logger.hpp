#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <spdlog/spdlog.h>

namespace game
{
class Logger
{
public:
  static void init();
  static void shutdown();
};
}
#endif
