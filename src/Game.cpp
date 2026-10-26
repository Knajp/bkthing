#include "Game.hpp"

#include "Logger.hpp"

namespace game
{
void Game::init()
{
  // Initialize game resources
  Logger::init();
  spdlog::info("Hello, World!");
  Window::initializeGLFW();
  spdlog::info("Initialized GLFW");
  mWindow.initWindow();
  spdlog::info("Initialized window");
  mRenderer.initRenderer(mWindow.getHandle());
  spdlog::info("Initialized renderer");
}

void Game::gameplayLoop()
{
  spdlog::info("Proceeding to gameplay loop");
  while(!mWindow.getWindowShouldClose())
  {
    VkCommandBuffer cb = mRenderer.beginRecording();
    mRenderer.beginRendering();

    vkCmdDraw(cb, 3, 1, 0, 0);

    mRenderer.finishRendering();
  }
  spdlog::info("Gameplay loop broken.");

}

void Game::terminate()
{
  spdlog::info("Starting game termination");
  // Destroy game resources
  mWindow.terminateWindow();
  Window::terminateGLFW();
  spdlog::info("Farewell, cruel world.");
  Logger::shutdown();

}

}
