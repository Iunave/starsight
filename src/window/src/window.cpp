#include "window.hpp"
#include "core/log.hpp"
#include "core/assertion.hpp"

namespace
{

void ErrorCallback(int error, const char* description)
{
    VERIFY(false, error, description);
}

void ToggleCursorCapture(GLFWwindow* window)
{
    int CurrentMode = glfwGetInputMode(window, GLFW_CURSOR);

    if(CurrentMode == GLFW_CURSOR_DISABLED)
    {
        LOG_INFO("setting input mode to GLFW_CURSOR_NORMAL");
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else
    {
        LOG_INFO("setting input mode to GLFW_CURSOR_DISABLED");
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(action == GLFW_PRESS)
    {
        if(mods & GLFW_MOD_CONTROL)
        {
            switch(key)
            {
                default:
                    break;
            }
        } else
        {
            switch(key)
            {
                case GLFW_KEY_ESCAPE:
                    ToggleCursorCapture(window);
                    break;
                default:
                    break;
            }
        }
    } else if(action == GLFW_RELEASE)
    {
        switch(key)
        {
            default:
                break;
        }
    }
}

}

GLFWwindow* GlfwCreateWindow(const char* title)
{
    LOG_INFO("creating window {}", title);

    GLFWwindow* Window = VERIFY(glfwCreateWindow(1000, 1000, title, nullptr, nullptr));
    glfwSetWindowUserPointer(Window, new GlfwWindowUserData{.WindowName = title});
    glfwSetKeyCallback(Window, &::KeyCallback);

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

    glfwSetErrorCallback(&::ErrorCallback);

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
