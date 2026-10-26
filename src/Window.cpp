#include "Window.hpp"

namespace game
{
bool Window::initializeGLFW()
{
  static bool init = false;
  if(init) return true;

  if(!glfwInit())
    return false;

  init = true;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  return true;
}
void Window::terminateGLFW()
{
  glfwTerminate();
}

void Window::initWindow()
{
  if(isInit) return;
  isInit = true;

  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* vidMode = glfwGetVideoMode(monitor);

  pWindow = glfwCreateWindow(vidMode->width, vidMode->height, "Spirit of Resistance", monitor, nullptr);
}
void Window::terminateWindow()
{
  glfwDestroyWindow(pWindow);
}
}
