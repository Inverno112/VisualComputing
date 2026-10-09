#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
} 

void processInput(GLFWwindow *window)
{
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}
 

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Create a windowed mode window and its OpenGL context
    //Variable have * because glfwCreateWindow returns a pointer to a GLFWwindow object
    //glfwCreateWindow takes 5 parameters: width, height, title, monitor, share
    //monitor is used for fullscreen mode, share is used to share resources between windows
    GLFWwindow *window = glfwCreateWindow(800,600, "LearnOpenGL, C++17", NULL, NULL);

    glViewport(0, 0, 800, 600);
    //glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);  

    while(!glfwWindowShouldClose(window))
    {
        glfwSwapBuffers(window);
        glfwPollEvents();    
    }
    glfwTerminate();
      
    return 0;
}