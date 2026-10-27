#include <glad/glad.h>//Biblioteca que contiene los punteros a las funciones de OpenGL.
// Se debe incluir antes de GLFW.
#include <GLFW/glfw3.h>//Biblioteca para crear ventanas y manejar eventos de entrada.
// Se debe incluir después de glad.
//Maneja la creación de ventanas, contextos OpenGL 
//y eventos de entrada (teclado, ratón, etc.).
//Un contexto OpenGL es un entorno donde se ejecutan las funciones de OpenGL 
//y se almacenan los estados y recursos gráficos.

#include <glm/glm.hpp>// Biblioteca para matemáticas de gráficos (vectores, matrices, etc.)
#include <glm/gtc/matrix_transform.hpp>// Biblioteca para transformaciones de matrices.

#include <learnopengl/shader.h>
// Biblioteca para manejar shaders de OpenGL.
#include <learnopengl/camera.h>
// Biblioteca para manejar la cámara.

#include "fbx_model.h"
// Biblioteca para cargar y renderizar modelos FBX.

#include <iostream>// Biblioteca para entrada/salida estándar.
#include <array>// Biblioteca para manejar arreglos de tamaño fijo.
#include <sstream>// Biblioteca para manejar flujos de texto.

// Funciones de callback y utilidades, declaradas aquí para mantener el main.cpp más limpio.
//GLFWwindow representa la ventana de GLFW.
//framebuffer_size_callback se llama cuando el tamaño del framebuffer cambia.
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
//mouse_callback se llama cuando el cursor del ratón se mueve.
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
//scroll_callback se llama cuando la rueda del ratón se desplaza.
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
//processInput se llama para procesar la entrada del teclado.
void processInput(GLFWwindow* window);
//mouse_button_callback se llama cuando se presiona o suelta un botón del ratón.
//button es el botón del ratón (GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT, etc.)
//action es la acción (GLFW_PRESS, GLFW_RELEASE).
//mods son las teclas modificadoras (Shift, Ctrl, Alt).
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
//drawOptionsUi dibuja la interfaz de usuario para las opciones de renderizado.
//Shader& es una referencia al shader que se usará para dibujar la UI. de tipo Shader,
// que es una clase definida en learnopengl/shader.h.
void drawOptionsUi(Shader& uiShader);
//updateWindowTitle actualiza el título de la ventana con el estado de las 
//opciones de renderizado.
void updateWindowTitle(GLFWwindow* window);

// settings (dinamicos para mantener el aspect ratio al redimensionar)
int scrWidth = 1280;
int scrHeight = 720;

// camera, define la posición y orientación de la cámara en el espacio 3D.
Camera camera(glm::vec3(0.0f, 0.5f, 4.0f));
// lastX y lastY almacenan la última posición del cursor del ratón.
float lastX = scrWidth / 2.0f;
float lastY = scrHeight / 2.0f;
bool firstMouse = true;
bool cameraDragging = false;

// Estructura para almacenar las opciones de renderizado.
struct RenderOptions
{
    bool unlit = false;
    bool directionalLight = true;
    bool pointLight = true;
    bool spotLight = true;
    bool specular = true;
    bool shadows = true;
    bool multipleLights = true;
    bool attenuation = true;
    bool ambientLight = true;
    bool ambientOcclusion = true;
} options;


// Variables para el framebuffer de sombras y el mapa de sombras.
//framebuffer: Es un objeto de OpenGL que permite renderizar a una textura en lugar de a la pantalla.
GLuint shadowFbo = 0;//representa el framebuffer de OpenGL que se utiliza para renderizar la 
//escena desde la perspectiva de la luz, generando un mapa de sombras.
GLuint shadowMap = 0;//representa la textura de OpenGL que almacena el mapa de sombras generado
// por el framebuffer shadowFbo.
constexpr unsigned int SHADOW_SIZE = 2048;// Tamaño del mapa de sombras (ancho y alto).
Shader* depthShader = nullptr;// Puntero al shader que se utiliza para renderizar la profundidad 
// desde la perspectiva de la luz.

//Al estar en NDC se adaptan automáticamente al tamaño de la ventana, manteniendo el aspect ratio.
constexpr int OPTION_COUNT = 10;// Número de opciones de renderizado disponibles.
constexpr float UI_LEFT = -0.97f;// Posición X del borde izquierdo de la UI en coordenadas NDC (Normalized Device Coordinates).
constexpr float UI_TOP = 0.90f;// Posición Y del borde superior de la UI en coordenadas NDC.
constexpr float UI_BUTTON_WIDTH = 0.09f;// Ancho de cada botón de la UI en coordenadas NDC.
constexpr float UI_BUTTON_HEIGHT = 0.08f;// Altura de cada botón de la UI en coordenadas NDC.
constexpr float UI_BUTTON_STEP = 0.105f;// Espacio entre botones de la UI en coordenadas NDC.

// timing
float deltaTime = 0.0f;// Tiempo entre el frame actual y el último frame.
float lastFrame = 0.0f;// Tiempo del último frame.

int main(int argc, char** argv)
{
    // Ruta del modelo FBX a cargar. Se puede pasar como argumento al ejecutar el programa.
    std::string modelPath = (argc > 1) ? argv[1] : "src/model.fbx";

    // Inicialización de GLFW y configuración de la ventana.
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);//define la versión mayor de OpenGL que se desea usar (3.x).
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);//define la versión menor de OpenGL que se desea usar (x.3).
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);//define el perfil de OpenGL que se desea usar (core profile).
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // Crear la ventana de GLFW con el tamaño especificado y el título "FBX Viewer".
    GLFWwindow* window = glfwCreateWindow(scrWidth, scrHeight, "FBX Viewer", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    // Hacer que el contexto de OpenGL de la ventana sea el actual.
    glfwMakeContextCurrent(window);
    // Registrar las funciones de callback para manejar eventos de la ventana y del ratón.
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // Registrar la función de callback para manejar el movimiento del ratón.
    glfwSetCursorPosCallback(window, mouse_callback);
    // Registrar la función de callback para manejar los botones del ratón.
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    // Registrar la función de callback para manejar el desplazamiento de la rueda del ratón.
    glfwSetScrollCallback(window, scroll_callback);
    // Configurar el modo de entrada del cursor del ratón. En este caso, 
    //se establece en modo normal (visible y libre).
    //existe otro modo llamado GLFW_CURSOR_DISABLED que oculta el cursor y lo bloquea en el centro de la ventana, 
    //útil para juegos en primera persona.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Inicializar GLAD para cargar las funciones de OpenGL.
    //gladLoadGLLoader es una función que carga los punteros a las funciones de 
    //OpenGL utilizando la función de carga proporcionada por GLFW (glfwGetProcAddress).
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Configuración de OpenGL: habilitar el test de profundidad y deshabilitar el culling.
    glEnable(GL_DEPTH_TEST);
    //El culling es una técnica de optimización que evita dibujar caras de un objeto que no son 
    //visibles para la cámara,
    //por ejemplo, las caras traseras de un cubo. Al deshabilitarlo, se dibujan todas las caras, 
    //lo que puede ser útil para depuración o para modelos
    glDisable(GL_CULL_FACE);

    // Cargar los shaders para renderizar el modelo FBX, la interfaz de usuario y el mapa de sombras.
    Shader shader("src/fbx.vs", "src/fbx.fs");// Shader para renderizar el modelo FBX.
    Shader uiShader("src/ui.vs", "src/ui.fs");// Shader para renderizar la interfaz de usuario.
    Shader shadowShader("src/depth.vs", "src/depth.fs");// Shader para renderizar la profundidad desde 
    //la perspectiva de la luz (mapa de sombras).
    depthShader = &shadowShader;// Asignar el shader de profundidad al puntero global depthShader.

    //Recordar que el framebuffer es un objeto de OpenGL que permite renderizar a una textura 
    //en lugar de a la pantalla.
    glGenFramebuffers(1, &shadowFbo);// Generar un framebuffer para renderizar el mapa de sombras.
    glGenTextures(1, &shadowMap);// Generar una textura para almacenar el mapa de sombras.
    glBindTexture(GL_TEXTURE_2D, shadowMap);// Vincular la textura del mapa de sombras como una textura 2D.
    // Configurar la textura del mapa de sombras: formato de profundidad, tamaño, filtros y envoltura.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);


    // Configurar los parámetros de la textura del mapa de sombras.
    //GL_TEXTURE_2D: Especifica que la textura es de tipo 2D.
    //GL_TEXTURE_MIN_FILTER: Especifica el filtro de minificación (cuando la textura se reduce).
    //GL_TEXTURE_MAG_FILTER: Especifica el filtro de magnificación (cuando la textura se amplía).
    //GL_TEXTURE_WRAP_S: Especifica el modo de envoltura en la coordenada S (horizontal).
    //GL_TEXTURE_WRAP_T: Especifica el modo de envoltura en la coordenada T (vertical).
    //Estas variables controlan cómo se comporta la textura cuando se aplica a un objeto 3D.
    //texel:  Es un "pixel" de una textura, es decir, la unidad más pequeña de información de color 
    //en una imagen.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
    glDrawBuffer(GL_NONE);// No se dibuja ningún color, solo se utiliza la textura de profundidad.
    glReadBuffer(GL_NONE);// No se lee ningún color, solo se utiliza la textura de profundidad.
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "No se pudo crear el framebuffer de sombras" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    FbxModel fbx;// Crear una instancia de la clase FbxModel para cargar y renderizar el modelo FBX.
    //fbx.load(modelPath) carga el modelo FBX desde la ruta especificada.
    //carga significa que se leen los datos del archivo FBX, se procesan y 
    //se almacenan en la estructura de datos de la clase FbxModel.
    //aún no se ha renderizado el modelo en la ventana, eso se hace en el bucle principal.
    //Para renderizar el modelo, se llama a la función fbx.draw() dentro del bucle principal.
    if (!fbx.load(modelPath))
    {
        std::cout << "No se pudo cargar: " << modelPath << std::endl;
        glfwTerminate();
        return -1;
    }

    // luz direccional + ambiente
    glm::vec3 lightDir = glm::normalize(glm::vec3(0.4f, 0.8f, 0.5f)); // hacia la luz
    glm::vec3 lightColor(1.0f);// color de la luz (blanco)
    float ambientStrength = 0.15f;// intensidad de la luz ambiental (0.0 a 1.0)
    glm::vec3 pointPositions[] = {// posiciones de las luces puntuales
        glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(-2.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 2.0f, -2.0f), glm::vec3(0.0f, -1.0f, 2.0f)
    };
    glm::vec3 pointColors[] = {// colores de las luces puntuales
        glm::vec3(1.0f, 0.25f, 0.15f), glm::vec3(0.15f, 0.35f, 1.0f),
        glm::vec3(0.2f, 1.0f, 0.35f), glm::vec3(1.0f, 0.65f, 0.15f)
    };

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);// Procesar la entrada del teclado y del ratón.

        glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
        // Establecer el color de fondo de la ventana (gris oscuro).
        // Limpiar el buffer de color y el buffer de profundidad para preparar el siguiente frame.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        //Calcular las matrices de proyección, vista y modelo para la cámara y la luz.
        //glm::perspective: crea una matriz de proyección en perspectiva.
        //radians: convierte grados a radianes. float scrWidth, scrHeight: dimensiones de la ventana.
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                                                (float)scrWidth / (float)scrHeight, 0.01f, 100.0f);
        //glm: view: crea una matriz de vista (cámara) usando la posición, el objetivo 
        //y el vector "up".
        glm::mat4 view = camera.GetViewMatrix();

        // Calcular la matriz de modelo para el objeto FBX (rotación alrededor del eje Y).
        //glm::rotate: crea una matriz de rotación alrededor de un eje dado.
        //recibe una matriz de identidad, un ángulo en radianes y 
        //un vector que representa el eje de rotación.
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), currentFrame * glm::radians(20.0f), glm::vec3(0, 1, 0));
        // Calcular la matriz de espacio de luz para el mapa de sombras.
        //glm::ortho: crea una matriz de proyección ortográfica. recibe los límites izquierdo, derecho,
        // inferior, superior, cercano y lejano del volumen de vista.
        glm::mat4 lightProjection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 20.0f);
        //glm::lookAt: crea una matriz de vista desde la posición de la luz hacia el origen.
        glm::mat4 lightView = glm::lookAt(-lightDir * 8.0f, glm::vec3(0.0f), glm::vec3(0, 1, 0));
        // Calcular la matriz de espacio de luz combinando la proyección y la vista de la luz.
        //multiplicación de matrices: la matriz de proyección se aplica primero,
        // seguida de la matriz de vista.
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        // Renderizar la escena desde la perspectiva de la luz para generar el mapa de sombras.
        //lo calculado antes permite renderizar la escena desde la perspectiva de la luz,
        // lo que permite generar un mapa de sombras.
        glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
        // Vincular el framebuffer de sombras y limpiar el buffer de profundidad.
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
        //limpiar el buffer de profundidad del framebuffer de sombras para preparar la renderización del mapa de sombras.
        glClear(GL_DEPTH_BUFFER_BIT);
        shadowShader.use();// Activar el shader de profundidad para renderizar la escena desde la 
        //perspectiva de la luz.
        shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        //shadowShader.setMat4("model", model) establece la matriz de modelo para el shader de profundidad,
        // que se utiliza para transformar las coordenadas del modelo FBX a las coordenadas de la luz.
        shadowShader.setMat4("model", model);
        // Renderizar el modelo FBX para generar el mapa de sombras.

        fbx.drawDepth();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, scrWidth, scrHeight);

        //envíar las matrices y parámetros de iluminación al shader principal para renderizar la escena.
        //Estos son sus uniforms, que se utilizan en el shader para calcular la iluminación, 
        //sombras y otros efectos.
        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        shader.setMat4("model", model);
        shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        shader.setVec3("viewPos", camera.Position);
        shader.setVec3("uLightDir", lightDir);
        shader.setVec3("uLightColor", lightColor);
        shader.setBool("uUnlit", options.unlit);
        shader.setFloat("uAmbientStrength", ambientStrength);
        shader.setFloat("uShininess", 32.0f);
        shader.setBool("uEnablePointLight", options.pointLight);
        shader.setBool("uEnableDirectionalLight", options.directionalLight);
        shader.setBool("uEnableSpotLight", options.spotLight);
        shader.setBool("uEnableSpecular", options.specular);
        shader.setBool("uEnableShadows", options.shadows);
        shader.setBool("uEnableAttenuation", options.attenuation);
        shader.setBool("uEnableAmbient", options.ambientLight);
        shader.setBool("uEnableAO", options.ambientOcclusion);
        shader.setInt("uPointLightCount", options.multipleLights ? 4 : 1);

        //para cada luz puntual, se envían sus posiciones y colores al shader.
        for (int i = 0; i < 4; ++i)
        {
            shader.setVec3("uPointLightPositions[" + std::to_string(i) + "]", pointPositions[i]);
            shader.setVec3("uPointLightColors[" + std::to_string(i) + "]", pointColors[i]);
        }

        // Configurar la luz direccional y la luz puntual en el shader.
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

        // Renderizar el modelo FBX con el shader principal.
        //en este caso el shader contiene fbx.vs y fbx.fs
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
        bool* values[] = { &options.unlit, &options.directionalLight, &options.pointLight,
                           &options.spotLight,
                           &options.specular,
                           &options.shadows, &options.multipleLights, &options.attenuation,
                           &options.ambientLight,
                           &options.ambientOcclusion };
        *values[index] = !*values[index];
        updateWindowTitle(window);
    }
}

void updateWindowTitle(GLFWwindow* window)
{
    std::ostringstream title;
    title << "FBX Viewer | Unlit:" << (options.unlit ? "ON" : "OFF")
          << " Directional light:" << (options.directionalLight ? "ON" : "OFF")
          << " Point light:" << (options.pointLight ? "ON" : "OFF")
          << " Spotlight:" << (options.spotLight ? "ON" : "OFF")// Spot light
          << " Specular:" << (options.specular ? "ON" : "OFF")// Specular
          << " Shadows:" << (options.shadows ? "ON" : "OFF")// Shadows
          << " Multiple lights:" << (options.multipleLights ? "ON" : "OFF")// Multiple lights
          << " Atenuation:" << (options.attenuation ? "ON" : "OFF")// Attenuation
          << " Ambient:" << (options.ambientLight ? "ON" : "OFF")
          << " AO:" << (options.ambientOcclusion ? "ON" : "OFF");
    glfwSetWindowTitle(window, title.str().c_str());
}

void drawOptionsUi(Shader& uiShader)
{
    const bool values[] = { options.unlit, options.directionalLight, options.pointLight,
                            options.spotLight,
                            options.specular,
                            options.shadows, options.multipleLights, options.attenuation,
                            options.ambientLight,
                            options.ambientOcclusion, };
    const glm::vec3 colorsOn[] = {
        {0.9f, 0.2f, 0.1f}, {0.9f, 0.6f, 0.1f}, {0.2f, 0.8f, 0.9f},
        {0.5f, 0.3f, 0.9f}, {0.2f, 0.8f, 0.3f}, {0.9f, 0.3f, 0.7f},
        {0.9f, 0.8f, 0.2f}, {0.3f, 0.8f, 0.7f}, {0.8f, 0.5f, 0.2f},
        {0.8f, 0.3f, 0.8f}
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