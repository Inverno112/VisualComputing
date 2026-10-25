# Flujo del programa

Este proyecto es un visor de modelos FBX desarrollado en C++ con GLFW,
GLAD, OpenGL, GLM y Assimp. El punto de entrada se encuentra en
[`src/main.cpp`](src/main.cpp).

## 1. Compilación y ejecución

1. El `Makefile` compila los archivos fuente de `src/` con C++17.
2. Se compilan `src/main.cpp` y `src/glad.c`.
3. El enlazador agrega GLFW, Assimp, OpenGL, GLM y las bibliotecas del
   sistema.
4. El ejecutable se genera como `output/main`.
5. Al ejecutar `make run`, se carga `src/model.fbx` por defecto.
   También puede indicarse otro modelo:

   ```bash
   make run MODEL=mi_modelo.fbx
   ```

## 2. Inicialización de la aplicación

El programa realiza las siguientes acciones en `main`:

1. Determina la ruta del modelo FBX a partir del primer argumento de la
   línea de comandos o utiliza `src/model.fbx`.
2. Inicializa GLFW y crea una ventana de 1280 x 720 con un contexto
   OpenGL 3.3 Core.
3. Registra los callbacks para:
   - redimensionar el framebuffer;
   - mover el mouse;
   - presionar botones del mouse;
   - usar la rueda del mouse.
4. Inicializa GLAD para obtener las funciones de OpenGL.
5. Activa el `depth test` y desactiva el `face culling`.
6. Carga y enlaza tres programas de shaders:
   - [`src/fbx.vs`](src/fbx.vs) y [`src/fbx.fs`](src/fbx.fs): modelo e
     iluminación;
   - [`src/depth.vs`](src/depth.vs) y [`src/depth.fs`](src/depth.fs):
     mapa de profundidad para sombras;
   - [`src/ui.vs`](src/ui.vs) y [`src/ui.fs`](src/ui.fs): botones de la
     interfaz.
7. Crea un framebuffer y una textura de profundidad de 2048 x 2048 para
   el mapa de sombras.
8. Carga el modelo mediante [`include/fbx_model.h`](include/fbx_model.h).
   Si la carga falla, se muestra un error y termina la aplicación.

## 3. Carga y preparación del modelo

La clase `FbxModel` utiliza Assimp para:

1. Leer el archivo FBX.
2. Triangular las caras, calcular normales, unir vértices y aplicar la
   transformación de la jerarquía del modelo.
3. Obtener la geometría, normales, coordenadas UV, tangentes y
   bitangentes de cada malla.
4. Leer el color difuso y buscar texturas difusas y normales, incluyendo
   texturas embebidas.
5. Calcular los límites del modelo, centrarlo en el origen y escalarlo
   aproximadamente a dos unidades.
6. Crear los recursos de GPU de cada malla:
   - VAO;
   - VBO;
   - EBO;
   - textura difusa;
   - textura normal.

Si una textura no puede cargarse, se utiliza una textura de color de
respaldo.

## 4. Bucle principal por frame

El programa repite el siguiente flujo mientras la ventana no solicite
cerrarse:

1. Obtiene el tiempo actual y calcula `deltaTime`.
2. Procesa el teclado mediante `processInput`:
   - `W`: mover la cámara hacia delante;
   - `S`: mover la cámara hacia atrás;
   - `A`: mover la cámara hacia la izquierda;
   - `D`: mover la cámara hacia la derecha;
   - `Esc`: cerrar la ventana.
3. Limpia los buffers de color y profundidad.
4. Calcula:
   - la matriz de proyección en perspectiva;
   - la matriz de vista de la cámara;
   - la matriz de modelo, que hace girar el objeto sobre el eje Y;
   - la matriz de vista-proyección de la luz.

### 4.1 Generación del mapa de sombras

Antes de dibujar la escena visible:

1. Cambia el viewport a 2048 x 2048.
2. Enlaza el framebuffer de sombras.
3. Limpia su buffer de profundidad.
4. Activa el shader de profundidad.
5. Dibuja todas las mallas con `FbxModel::drawDepth`.
6. Guarda en `shadowMap` la profundidad observada desde la dirección de
   la luz.
7. Vuelve al framebuffer de la ventana y restaura su viewport.

### 4.2 Renderizado del modelo

1. Activa el shader principal.
2. Envía las matrices de cámara, modelo y luz.
3. Envía la posición de la cámara y los parámetros de iluminación.
4. Configura una luz direccional, hasta cuatro luces puntuales y una luz
   tipo foco asociada a la cámara.
5. Envía las opciones activas de renderizado:
   - luz puntual;
   - luz de foco;
   - iluminación especular;
   - sombras;
   - múltiples luces puntuales;
   - atenuación;
   - oclusión ambiental.
6. Vincula el mapa de sombras en la unidad de textura 2.
7. `FbxModel::draw` recorre las mallas, vincula sus texturas y dibuja
   sus índices como triángulos.

El vertex shader transforma los vértices al espacio de pantalla y
calcula la posición, normal, coordenadas UV y matriz TBN. El fragment
shader:

1. Aplica la textura normal cuando existe.
2. Obtiene el color difuso desde la textura o desde el material.
3. Calcula las sombras mediante PCF sobre el mapa de profundidad.
4. Suma iluminación ambiental, direccional, puntual y de foco.
5. Aplica iluminación especular, atenuación y oclusión ambiental según
   las opciones activas.
6. Escribe el color final del fragmento.

### 4.3 Interfaz y presentación

1. `drawOptionsUi` dibuja siete botones en la parte superior de la
   ventana usando coordenadas NDC.
2. Cada botón cambia de color según la opción correspondiente esté
   activa o desactivada.
3. Se intercambian los buffers con `glfwSwapBuffers`.
4. Se procesan los eventos pendientes con `glfwPollEvents`.

## 5. Interacción con el mouse

- Mantener presionado el botón derecho y mover el mouse rota la cámara.
- Usar la rueda modifica el zoom de la cámara.
- Presionar el botón izquierdo sobre uno de los siete botones cambia su
  estado y actualiza el título de la ventana.
- Al redimensionar la ventana se actualizan `scrWidth`, `scrHeight` y
  el viewport para conservar la relación de aspecto.

## 6. Finalización

Cuando se presiona `Esc` o se cierra la ventana:

1. Termina el bucle principal.
2. Se destruye el contexto de GLFW.
3. Se liberan el framebuffer y la textura del mapa de sombras.
4. El destructor de `FbxModel` libera los VAO, VBO, EBO y texturas de
   cada malla.
5. El programa termina con código `0`.

## Resumen del flujo

```text
Compilar
   ↓
Inicializar GLFW, OpenGL y GLAD
   ↓
Cargar shaders, framebuffer de sombras y modelo FBX
   ↓
┌─ Mientras la ventana esté abierta ──────────────────────────┐
│ Leer tiempo y entradas                                       │
│ Calcular matrices de cámara, modelo y luz                    │
│ Renderizar profundidad desde la luz                          │
│ Renderizar el modelo con iluminación y sombras               │
│ Dibujar botones de opciones                                  │
│ Intercambiar buffers y procesar eventos                      │
└──────────────────────────────────────────────────────────────┘
   ↓
Liberar recursos y terminar
```