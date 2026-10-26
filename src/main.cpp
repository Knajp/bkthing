#include <iostream>
#include "Game.hpp"
int main(int argc, char** argv)
{
  game::Game& instance = game::Game::getInstance();
  try{
    instance.RUN();
  }catch(const std::runtime_error& err)
      {std::cerr << err.what() << "\n";}
  return 0;
}
