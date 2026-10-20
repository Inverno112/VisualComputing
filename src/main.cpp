#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>

#include "fbx_model.h"

#include <iostream>
#include <array>
#include <sstream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void drawOptionsUi(Shader& uiShader);
void updateWindowTitle(GLFWwindow* window);

// settings (dinamicos para mantener el aspect ratio al redimensionar)
int scrWidth = 1280;
int scrHeight = 720;

// camera
Camera camera(glm::vec3(0.0f, 0.5f, 4.0f));
float lastX = scrWidth / 2.0f;
float lastY = scrHeight / 2.0f;
bool firstMouse = true;
bool cameraDragging = false;

struct RenderOptions
{
    bool pointLight = true;
    bool spotLight = true;
    bool specular = true;
    bool shadows = true;
    bool multipleLights = true;
    bool attenuation = true;
    bool ambientOcclusion = true;
} options;

GLuint shadowFbo = 0;
GLuint shadowMap = 0;
constexpr unsigned int SHADOW_SIZE = 2048;
Shader* depthShader = nullptr;

constexpr int OPTION_COUNT = 7;
constexpr float UI_LEFT = -0.97f;
constexpr float UI_TOP = 0.90f;
constexpr float UI_BUTTON_WIDTH = 0.09f;
constexpr float UI_BUTTON_HEIGHT = 0.08f;
constexpr float UI_BUTTON_STEP = 0.105f;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main(int argc, char** argv)
{
    // Uso: fbx_viewer ruta/al/modelo.fbx
    std::string modelPath = (argc > 1) ? argv[1] : "src/model.fbx";

    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(scrWidth, scrHeight, "FBX Viewer", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    // Sin culling: muchos FBX traen winding inconsistente. El shader voltea la normal con gl_FrontFacing.
    glDisable(GL_CULL_FACE);

    Shader shader("src/fbx.vs", "src/fbx.fs");
    Shader uiShader("src/ui.vs", "src/ui.fs");
    Shader shadowShader("src/depth.vs", "src/depth.fs");
    depthShader = &shadowShader;

    glGenFramebuffers(1, &shadowFbo);
    glGenTextures(1, &shadowMap);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "No se pudo crear el framebuffer de sombras" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    FbxModel fbx;
    if (!fbx.load(modelPath))
    {
        std::cout << "No se pudo cargar: " << modelPath << std::endl;
        glfwTerminate();
        return -1;
    }

    // luz direccional + ambiente
    glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 0.8f, 0.5f)); // hacia la luz
    glm::vec3 lightColor(1.0f);
    float ambientStrength = 0.15f;
    glm::vec3 pointPositions[] = {
        glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(-2.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 2.0f, -2.0f), glm::vec3(0.0f, -1.0f, 2.0f)
    };
    glm::vec3 pointColors[] = {
        glm::vec3(1.0f, 0.25f, 0.15f), glm::vec3(0.15f, 0.35f, 1.0f),
        glm::vec3(0.2f, 1.0f, 0.35f), glm::vec3(1.0f, 0.65f, 0.15f)
    };

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                                                (float)scrWidth / (float)scrHeight, 0.01f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), currentFrame * glm::radians(20.0f), glm::vec3(0, 1, 0));
        glm::mat4 lightProjection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 20.0f);
        glm::mat4 lightView = glm::lookAt(-lightDir * 8.0f, glm::vec3(0.0f), glm::vec3(0, 1, 0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
        glClear(GL_DEPTH_BUFFER_BIT);
        shadowShader.use();
        shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        shadowShader.setMat4("model", model);
        fbx.drawDepth();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, scrWidth, scrHeight);

        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        shader.setMat4("model", model);
        shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        shader.setVec3("viewPos", camera.Position);
        shader.setVec3("uLightDir", lightDir);
        shader.setVec3("uLightColor", lightColor);
        shader.setFloat("uAmbientStrength", ambientStrength);
        shader.setFloat("uShininess", 32.0f);
        shader.setBool("uEnablePointLight", options.pointLight);
        shader.setBool("uEnableSpotLight", options.spotLight);
        shader.setBool("uEnableSpecular", options.specular);
        shader.setBool("uEnableShadows", options.shadows);
        shader.setBool("uEnableAttenuation", options.attenuation);
        shader.setBool("uEnableAO", options.ambientOcclusion);
        shader.setInt("uPointLightCount", options.multipleLights ? 4 : 1);
        for (int i = 0; i < 4; ++i)
        {
            shader.setVec3("uPointLightPositions[" + std::to_string(i) + "]", pointPositions[i]);
            shader.setVec3("uPointLightColors[" + std::to_string(i) + "]", pointColors[i]);
        }
        shader.setVec3("uSpotLightPos", camera.Position);
        shader.setVec3("uSpotLightDir", camera.Front);
        shader.setVec3("uSpotLightColor", glm::vec3(1.0f, 0.85f, 0.55f));
        shader.setFloat("uSpotInnerCutoff", glm::cos(glm::radians(12.5f)));
        shader.setFloat("uSpotOuterCutoff", glm::cos(glm::radians(17.5f)));
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, shadowMap);
        shader.setInt("uShadowMap", 2);

        // hooks Beer-Lambert (apagado por defecto)
        shader.setBool("uEnableBeer", false);
        shader.setVec3("uAbsorption", glm::vec3(0.4f, 0.1f, 0.05f));

        fbx.draw(shader);
        drawOptionsUi(uiShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    glDeleteFramebuffers(1, &shadowFbo);
    glDeleteTextures(1, &shadowMap);
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow*, int width, int height)
{
    scrWidth = width;
    scrHeight = height > 0 ? height : 1;
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow*, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    if (cameraDragging)
        camera.ProcessMouseMovement(xpos - lastX, lastY - ypos);
    lastX = xpos;
    lastY = ypos;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        cameraDragging = action == GLFW_PRESS;
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

    double x, y;
    glfwGetCursorPos(window, &x, &y);

    // GLFW entrega la posición del cursor en coordenadas de ventana. Convertir
    // a NDC hace que la zona de escucha coincida con los quads dibujados.
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0) return;
    const float mouseNdcX = 2.0f * static_cast<float>(x) / windowWidth - 1.0f;
    const float mouseNdcY = 1.0f - 2.0f * static_cast<float>(y) / windowHeight;
    const int index = static_cast<int>((mouseNdcX - UI_LEFT) / UI_BUTTON_STEP);
    const float buttonLeft = UI_LEFT + index * UI_BUTTON_STEP;
    if (mouseNdcY >= UI_TOP && mouseNdcY <= UI_TOP + UI_BUTTON_HEIGHT &&
        index >= 0 && index < OPTION_COUNT &&
        mouseNdcX >= buttonLeft && mouseNdcX <= buttonLeft + UI_BUTTON_WIDTH)
    {
        bool* values[] = { &options.pointLight, &options.spotLight, &options.specular,
                           &options.shadows, &options.multipleLights, &options.attenuation,
                           &options.ambientOcclusion };
        *values[index] = !*values[index];
        updateWindowTitle(window);
    }
}

void updateWindowTitle(GLFWwindow* window)
{
    std::ostringstream title;
    title << "FBX Viewer | P:" << (options.pointLight ? "ON" : "OFF")
          << " S:" << (options.spotLight ? "ON" : "OFF")
          << " V:" << (options.specular ? "ON" : "OFF")
          << " H:" << (options.shadows ? "ON" : "OFF")
          << " M:" << (options.multipleLights ? "ON" : "OFF")
          << " A:" << (options.attenuation ? "ON" : "OFF")
          << " O:" << (options.ambientOcclusion ? "ON" : "OFF");
    glfwSetWindowTitle(window, title.str().c_str());
}

void drawOptionsUi(Shader& uiShader)
{
    const bool values[] = { options.pointLight, options.spotLight, options.specular,
                            options.shadows, options.multipleLights, options.attenuation,
                            options.ambientOcclusion };
    const glm::vec3 colorsOn[] = {
        {0.9f, 0.2f, 0.1f}, {0.9f, 0.6f, 0.1f}, {0.2f, 0.8f, 0.9f},
        {0.5f, 0.3f, 0.9f}, {0.2f, 0.8f, 0.3f}, {0.9f, 0.3f, 0.7f}, {0.3f, 0.8f, 0.7f}
    };
    std::vector<float> vertices;
    for (int i = 0; i < OPTION_COUNT; ++i)
    {
        float x0 = UI_LEFT + i * UI_BUTTON_STEP;
        float x1 = x0 + UI_BUTTON_WIDTH;
        float y0 = UI_TOP, y1 = UI_TOP + UI_BUTTON_HEIGHT;
        glm::vec3 color = values[i] ? colorsOn[i] : glm::vec3(0.15f);
        const float quad[] = { x0,y0,color.r,color.g,color.b, x1,y0,color.r,color.g,color.b,
                               x1,y1,color.r,color.g,color.b, x0,y0,color.r,color.g,color.b,
                               x1,y1,color.r,color.g,color.b, x0,y1,color.r,color.g,color.b };
        vertices.insert(vertices.end(), std::begin(quad), std::end(quad));
    }
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glDisable(GL_DEPTH_TEST);
    uiShader.use();
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 5));
    glEnable(GL_DEPTH_TEST);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

void scroll_callback(GLFWwindow*, double, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}