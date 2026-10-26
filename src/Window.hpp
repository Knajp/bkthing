#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <GLFW/glfw3.h>

namespace game
{

class Window 
{
public:
  Window() = default;
  
  static bool initializeGLFW();
  static void terminateGLFW();

  void initWindow();
  void terminateWindow();
  
  GLFWwindow* getHandle() const {return pWindow;}
  bool getWindowShouldClose() const {return glfwWindowShouldClose(pWindow);}
private:
  GLFWwindow* pWindow = nullptr;
  bool isInit = false;
};
}
#endif 
