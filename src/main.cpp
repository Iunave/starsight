#include <pthread.h>

#include "window/window.hpp"
#include "render/vk_context.hpp"

int main()
{
    pthread_setname_np(pthread_self(), "main");

    GlfwInitialize();
    GLFWwindow* Window = GlfwCreateWindow("starsight");

    vkContext = new VContext{};

    while(!glfwWindowShouldClose(Window))
    {
        glfwPollEvents();
    }

    delete vkContext;
    vkContext = nullptr;

    GlfwCloseWindow(Window);
    GlfwTerminate();

    return 0;
}
