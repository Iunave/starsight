#ifndef STARSIGHT_WINDOW_HPP
#define STARSIGHT_WINDOW_HPP

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include <string>

namespace global
{
    inline constinit GLFWwindow* Window = nullptr;
}

struct GlfwWindowUserData
{
    std::string WindowName{};
};

void GlfwErrorCallback(int error = 0, const char* description = nullptr);
GlfwWindowUserData* GlfwGetWindowUserData(GLFWwindow* Window);
void GlfwInitialize();
void GlfwTerminate();
GLFWwindow* GlfwCreateWindow(const char* title);
void GlfwCloseWindow(GLFWwindow* window);

#endif //STARSIGHT_WINDOW_HPP
