#ifndef GAME_HPP 
#define GAME_HPP

#include "Window.hpp"
#include "Rend/Renderer.hpp"
namespace game
{
class Game
{
public:
  static Game& getInstance()
  {
    static Game instance;
    return instance;
  }

  void RUN()
  {
    init();
    gameplayLoop();
    terminate();
  }
private:
  void init();
  void gameplayLoop();
  void terminate();

private:
  Window mWindow;
  Renderer mRenderer = Renderer::getInstance();

};
}
#endif 
