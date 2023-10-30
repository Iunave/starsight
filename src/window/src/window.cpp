#include "window.hpp"
#include "core/log.hpp"
#include "core/assertion.hpp"

void GlfwErrorCallback(int error, const char* description)
{
    VERIFY(false, error, description);
}

GLFWwindow* GlfwCreateWindow(const char* title)
{
    LOG_INFO("creating window {}", title);

    GLFWwindow* Window = VERIFY(glfwCreateWindow(1000, 1000, title, nullptr, nullptr));
    glfwSetWindowUserPointer(Window, new GlfwWindowUserData{.WindowName = title});
    return Window;
}

void GlfwCloseWindow(GLFWwindow* window)
{
    GlfwWindowUserData* UserData = GlfwGetWindowUserData(window);

    LOG_INFO("closing window {}", UserData->WindowName);

    delete UserData;
    glfwDestroyWindow(window);
}

void GlfwTerminate()
{
    LOG_INFO("terminating glfw");
    glfwTerminate();
}

void GlfwInitialize()
{
    LOG_INFO("initializing glfw");

    glfwSetErrorCallback(&::GlfwErrorCallback);

    VERIFY(glfwInit());
    VERIFY(glfwVulkanSupported());

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
}

GlfwWindowUserData* GlfwGetWindowUserData(GLFWwindow* Window)
{
    void* UserData = glfwGetWindowUserPointer(Window);
    return static_cast<GlfwWindowUserData*>(UserData);
}
