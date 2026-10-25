# Flujo del programa y funciones por fase

El programa es un visor de modelos FBX estáticos. Su flujo se divide en
inicialización, carga del modelo, procesamiento de entrada, generación de
sombras, renderizado, interfaz y liberación de recursos.

El punto de entrada es `main()` en
[`src/main.cpp`](src/main.cpp).

## 1. Compilación y entrada al programa

### Funciones involucradas

| Función o regla | Archivo | Responsabilidad |
|---|---|---|
| Regla `all` | [`Makefile`](Makefile) | Crea el directorio de salida y enlaza el ejecutable. |
| Regla `.cpp.o` | [`Makefile`](Makefile) | Compila los archivos C++ con C++17. |
| Regla de `glad.c` | [`Makefile`](Makefile) | Compila las funciones cargadoras de OpenGL. |
| `main(int argc, char** argv)` | [`src/main.cpp`](src/main.cpp) | Recibe opcionalmente la ruta del modelo FBX. |

La ruta utilizada se obtiene así:

```cpp
std::string modelPath = (argc > 1) ? argv[1] : "src/model.fbx";
```

Si no se proporciona un argumento, se utiliza
[`src/model.fbx`](src/model.fbx).

## 2. Creación de la ventana y del contexto OpenGL

Esta fase prepara el entorno donde se ejecutará el renderizado.

### Funciones necesarias

| Función | Librería | Responsabilidad |
|---|---|---|
| `glfwInit()` | GLFW | Inicializa la biblioteca de ventanas y eventos. |
| `glfwWindowHint()` | GLFW | Solicita OpenGL 3.3 Core Profile. |
| `glfwCreateWindow()` | GLFW | Crea la ventana de 1280 x 720. |
| `glfwMakeContextCurrent()` | GLFW | Hace activo el contexto OpenGL de la ventana. |
| `glfwSetFramebufferSizeCallback()` | GLFW | Registra `framebuffer_size_callback`. |
| `glfwSetCursorPosCallback()` | GLFW | Registra `mouse_callback`. |
| `glfwSetMouseButtonCallback()` | GLFW | Registra `mouse_button_callback`. |
| `glfwSetScrollCallback()` | GLFW | Registra `scroll_callback`. |
| `glfwSetInputMode()` | GLFW | Mantiene visible y libre el cursor. |
| `gladLoadGLLoader()` | GLAD | Carga los punteros a las funciones de OpenGL. |
| `glEnable(GL_DEPTH_TEST)` | OpenGL | Activa la prueba de profundidad. |
| `glDisable(GL_CULL_FACE)` | OpenGL | Permite dibujar las caras frontal y posterior. |

Si falla `glfwInit`, `glfwCreateWindow` o `gladLoadGLLoader`, `main`
informa el error y termina.

## 3. Carga y configuración de shaders

La clase `Shader` está definida en
[`include/learnopengl/shader.h`](include/learnopengl/shader.h).

### Funciones necesarias de `Shader`

| Función | Responsabilidad |
|---|---|
| `Shader::Shader(vertexPath, fragmentPath)` | Lee los archivos, crea y compila los shaders y enlaza el programa. |
| `Shader::checkCompileErrors()` | Comprueba errores de compilación y enlazado. |
| `Shader::use()` | Activa el programa de shaders. |
| `Shader::setBool()` | Envía opciones booleanas como uniforms. |
| `Shader::setInt()` | Envía índices de textura y contadores. |
| `Shader::setFloat()` | Envía intensidades, ángulos y otros escalares. |
| `Shader::setVec3()` | Envía posiciones, colores y direcciones. |
| `Shader::setMat4()` | Envía matrices de transformación. |

En `main()` se crean tres programas:

```cpp
Shader shader("src/fbx.vs", "src/fbx.fs");
Shader uiShader("src/ui.vs", "src/ui.fs");
Shader shadowShader("src/depth.vs", "src/depth.fs");
```

- `shader` renderiza el modelo con iluminación.
- `uiShader` renderiza los botones de opciones.
- `shadowShader` genera únicamente la profundidad desde la luz.

## 4. Creación del framebuffer de sombras

El framebuffer permite dibujar la escena en una textura de profundidad en
lugar de dibujarla directamente en pantalla.

### Funciones OpenGL necesarias

| Función | Responsabilidad |
|---|---|
| `glGenFramebuffers()` | Crea `shadowFbo`. |
| `glGenTextures()` | Crea `shadowMap`. |
| `glBindTexture()` | Activa la textura de profundidad. |
| `glTexImage2D()` | Reserva una textura `GL_DEPTH_COMPONENT` de 2048 x 2048. |
| `glTexParameteri()` | Configura filtros y bordes de la textura. |
| `glBindFramebuffer()` | Selecciona el framebuffer de sombras. |
| `glFramebufferTexture2D()` | Asocia `shadowMap` al attachment de profundidad. |
| `glDrawBuffer(GL_NONE)` | Desactiva la salida de color. |
| `glReadBuffer(GL_NONE)` | Desactiva la lectura de color. |
| `glCheckFramebufferStatus()` | Verifica que el framebuffer esté completo. |

## 5. Carga y preparación del modelo FBX

La clase `FbxModel` se encuentra en
[`include/fbx_model.h`](include/fbx_model.h).

### Función principal

```cpp
bool FbxModel::load(const std::string& path);
```

Esta función ejecuta el siguiente subflujo:

1. `Assimp::Importer::ReadFile()` abre el modelo con estas operaciones:
   triangulación, normales suaves, unión de vértices, tangentes y
   transformación de nodos.
2. Comprueba que la escena exista, no esté incompleta y tenga nodo raíz.
3. Recorre `scene->mMeshes`.
4. Para cada malla:
   - obtiene el color del material;
   - busca las texturas del material mediante `texturePath()`;
   - copia posición, normal, UV, tangente y bitangente;
   - copia los índices de las caras triangulares;
   - actualiza los límites `bmin` y `bmax`.
5. Calcula el centro y el factor de escala para normalizar el modelo.
6. Crea los recursos de GPU de cada malla.

### Funciones auxiliares de carga

| Función | Responsabilidad |
|---|---|
| `FbxModel::texturePath()` | Resuelve la ruta de una textura externa o embebida. |
| `FbxModel::loadTexture()` | Carga una imagen con `stbi_load`, crea una textura OpenGL y genera mipmaps. |
| `stbi_load()` / `stbi_load_from_memory()` | Obtienen los píxeles de texturas externas o embebidas. |
| `glGenVertexArrays()` | Crea el VAO de la malla. |
| `glGenBuffers()` | Crea VBO y EBO. |
| `glBufferData()` | Copia vértices e índices a la GPU. |
| `glVertexAttribPointer()` | Define los atributos de posición, normal, UV, tangente y bitangente. |

Cada vértice utiliza 14 valores `float`:

```text
posición(3) + normal(3) + UV(2) + tangente(3) + bitangente(3)
```

Si no se puede cargar una textura, `loadTexture()` crea un píxel de
respaldo: blanco para la textura difusa y azul normal para la textura
normal.

## 6. Configuración de la cámara y de las luces

La cámara global es una instancia de `Camera`, definida en
[`include/learnopengl/camera.h`](include/learnopengl/camera.h).

### Funciones de `Camera`

| Función | Responsabilidad |
|---|---|
| `Camera::Camera()` | Inicializa posición, orientación, velocidad, sensibilidad y zoom. |
| `Camera::GetViewMatrix()` | Devuelve la matriz de vista con `glm::lookAt`. |
| `Camera::ProcessKeyboard()` | Mueve la posición con `Front` o `Right`. |
| `Camera::ProcessMouseMovement()` | Actualiza `Yaw` y `Pitch` y limita el `Pitch`. |
| `Camera::ProcessMouseScroll()` | Cambia `Zoom` entre 1 y 45 grados. |
| `Camera::updateCameraVectors()` | Recalcula los vectores `Front`, `Right` y `Up`. |

En `main()` también se definen:

- una luz direccional (`lightDir`, `lightColor`);
- cuatro luces puntuales (`pointPositions`, `pointColors`);
- una luz tipo foco cuya posición y dirección corresponden a la cámara;
- la intensidad de luz ambiental.

## 7. Procesamiento de entradas

La función `processInput(GLFWwindow* window)` se ejecuta una vez por
frame.

### Funciones de entrada

| Función | Evento o acción |
|---|---|
| `processInput()` | Comprueba teclado y solicita el cierre de la ventana. |
| `glfwGetKey()` | Lee `Esc`, `W`, `S`, `A` y `D`. |
| `glfwSetWindowShouldClose()` | Marca la ventana para finalizar el bucle. |
| `camera.ProcessKeyboard()` | Aplica el movimiento de la cámara. |
| `mouse_callback()` | Convierte el desplazamiento del cursor en rotación. |
| `mouse_button_callback()` | Activa el arrastre de cámara o cambia opciones de UI. |
| `scroll_callback()` | Convierte la rueda en zoom. |
| `framebuffer_size_callback()` | Actualiza el tamaño y el viewport. |

### Flujo del mouse

- Botón derecho presionado: `cameraDragging = true`.
- Movimiento con arrastre: `camera.ProcessMouseMovement()`.
- Rueda: `camera.ProcessMouseScroll()`.
- Botón izquierdo: convierte la posición con NDC, calcula el botón y
  alterna uno de los valores de `options`.
- Cambio de opción: `updateWindowTitle()` refleja el estado en el título.

## 8. Preparación de matrices por frame

Dentro de `while (!glfwWindowShouldClose(window))`, `main()` llama o
calcula:

| Función o expresión | Resultado |
|---|---|
| `glfwGetTime()` | Tiempo actual para calcular `deltaTime` y rotación. |
| `glm::perspective()` | Matriz de proyección de la cámara. |
| `camera.GetViewMatrix()` | Matriz de vista. |
| `glm::rotate()` | Matriz de modelo que rota el FBX sobre Y. |
| `glm::ortho()` | Proyección ortográfica de la luz. |
| `glm::lookAt()` | Vista desde la posición de la luz. |
| `lightProjection * lightView` | `lightSpaceMatrix` para sombras. |

## 9. Primera pasada: generación de sombras

Esta pasada dibuja la escena desde la perspectiva de la luz.

### Secuencia de funciones

1. `glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE)` configura la resolución
   del mapa.
2. `glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo)` selecciona el
   framebuffer de sombras.
3. `glClear(GL_DEPTH_BUFFER_BIT)` limpia la profundidad anterior.
4. `shadowShader.use()` activa el shader de profundidad.
5. `shadowShader.setMat4()` envía `lightSpaceMatrix` y `model`.
6. `FbxModel::drawDepth()` recorre las mallas.
7. `glDrawElements()` dibuja sus triángulos usando únicamente la
   profundidad.
8. `glBindFramebuffer(GL_FRAMEBUFFER, 0)` vuelve a la ventana.
9. `glViewport(0, 0, scrWidth, scrHeight)` restaura el viewport normal.

El vertex shader de profundidad ejecuta:

```glsl
gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
```

## 10. Segunda pasada: renderizado del modelo

### Configuración desde `main()`

1. `shader.use()` activa el shader principal.
2. `shader.setMat4()` envía las matrices `projection`, `view`, `model` y
   `lightSpaceMatrix`.
3. `shader.setVec3()` envía cámara, direcciones y colores.
4. `shader.setFloat()` envía intensidad ambiental, brillo y cortes del
   foco.
5. `shader.setBool()` activa o desactiva las opciones de renderizado.
6. `shader.setInt()` define el número de luces puntuales y las unidades
   de textura.
7. `glActiveTexture()` y `glBindTexture()` vinculan el mapa de sombras.
8. `fbx.draw(shader)` dibuja el modelo.

### Funciones de `FbxModel::draw`

Para cada `GpuMesh`, `draw()`:

1. Envía `uDiffuseColor`, `uHasDiffuseTexture` y
   `uHasNormalTexture`.
2. Vincula la textura difusa en la unidad 0.
3. Vincula la textura normal en la unidad 1.
4. Vincula el VAO.
5. Llama a `glDrawElements(GL_TRIANGLES, ...)`.
6. Desvincula el VAO al terminar.

### Funciones del vertex shader

En [`src/fbx.vs`](src/fbx.vs):

| Función o operación | Responsabilidad |
|---|---|
| `main()` | Transformar cada vértice y preparar datos para el fragment shader. |
| `transpose(inverse(model))` | Transformar correctamente las normales. |
| `normalize()` | Normalizar normales y vectores tangentes. |
| `cross()` | Calcular la bitangente y construir la base TBN. |

El shader genera `FragPos`, `Normal`, `TexCoords`, `TBN` y
`FragPosLightSpace`.

### Funciones del fragment shader

En [`src/fbx.fs`](src/fbx.fs):

| Función | Responsabilidad |
|---|---|
| `shadowFactor()` | Compara la profundidad actual con `uShadowMap` y aplica PCF 3 x 3. |
| `specularLight()` | Calcula la componente especular con Blinn-Phong. |
| `attenuation()` | Reduce la intensidad según la distancia a la luz. |
| `main()` | Combina material, ambiente, luces, sombras, especular y AO. |

Dentro de `main()` del fragment shader se ejecuta este orden:

1. Normalizar la normal del fragmento.
2. Aplicar la textura normal mediante la matriz TBN, si existe.
3. Obtener el albedo desde la textura difusa o el color del material.
4. Calcular la sombra con `shadowFactor()`, si está habilitada.
5. Calcular la iluminación ambiental y el AO hemisférico.
6. Sumar luz direccional y su componente especular.
7. Recorrer las luces puntuales y aplicar `attenuation()`.
8. Calcular la luz de foco asociada a la cámara.
9. Escribir `FragColor`.

## 11. Renderizado de la interfaz

La función `drawOptionsUi(Shader& uiShader)` crea y dibuja siete botones.

### Funciones necesarias

| Función | Responsabilidad |
|---|---|
| `drawOptionsUi()` | Construye los quads y dibuja la UI. |
| `glGenVertexArrays()` / `glGenBuffers()` | Crean VAO y VBO temporales. |
| `glBufferData()` | Copia posiciones y colores de los botones. |
| `glVertexAttribPointer()` | Define posición 2D y color 3D. |
| `uiShader.use()` | Activa el shader de UI. |
| `glDisable(GL_DEPTH_TEST)` | Hace que la UI se dibuje sobre el modelo. |
| `glDrawArrays(GL_TRIANGLES, ...)` | Dibuja los siete quads. |
| `glEnable(GL_DEPTH_TEST)` | Restaura la prueba de profundidad. |
| `glDeleteBuffers()` / `glDeleteVertexArrays()` | Liberan los recursos temporales. |

El vertex shader de UI (`src/ui.vs`) pasa las posiciones directamente a
NDC y el fragment shader (`src/ui.fs`) dibuja el color con alfa `0.92`.

## 12. Presentación del frame

Al terminar las dos pasadas de renderizado y la UI:

```cpp
glfwSwapBuffers(window);
glfwPollEvents();
```

- `glfwSwapBuffers()` muestra el back buffer ya renderizado.
- `glfwPollEvents()` entrega a GLFW los eventos de teclado, mouse y
  ventana, que pueden invocar los callbacks registrados.

Después el flujo vuelve al inicio del `while`.

## 13. Finalización y liberación de recursos

Cuando `glfwWindowShouldClose(window)` devuelve `true`:

1. `glfwTerminate()` finaliza GLFW.
2. `glDeleteFramebuffers()` libera `shadowFbo`.
3. `glDeleteTextures()` libera `shadowMap`.
4. El destructor `FbxModel::~FbxModel()` libera por cada malla:
   - VAO mediante `glDeleteVertexArrays()`;
   - VBO y EBO mediante `glDeleteBuffers()`;
   - texturas mediante `glDeleteTextures()`.
5. `main()` retorna `0`.

## Resumen de llamadas

```text
main()
 ├─ glfwInit()
 ├─ glfwCreateWindow()
 ├─ gladLoadGLLoader()
 ├─ Shader::Shader() x3
 ├─ glGenFramebuffers() / glGenTextures()
 ├─ FbxModel::load()
 │   ├─ Assimp::Importer::ReadFile()
 │   ├─ texturePath()
 │   ├─ loadTexture()
 │   └─ creación de VAO/VBO/EBO
 └─ while (!glfwWindowShouldClose())
     ├─ glfwGetTime()
     ├─ processInput()
     │   └─ Camera::ProcessKeyboard()
     ├─ glm::perspective() / GetViewMatrix() / glm::rotate()
     ├─ glm::ortho() / glm::lookAt()
     ├─ shadowShader.use()
     ├─ FbxModel::drawDepth()
     ├─ shader.use() / Shader::set*
     ├─ FbxModel::draw()
     │   └─ glDrawElements()
     ├─ drawOptionsUi()
     │   └─ glDrawArrays()
     ├─ glfwSwapBuffers()
     └─ glfwPollEvents()
```
