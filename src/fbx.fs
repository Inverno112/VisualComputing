#version 330 core

//este shader tiene las siguientes funciones de luz:
//shadowFactor: calcula el factor de sombra para un fragmento dado, utilizando el mapa de sombras generado desde la perspectiva de la luz.
//specularLight: calcula la iluminación especular para un fragmento dado, utilizando el modelo de iluminación de Blinn-Phong.
//attenuation: calcula la atenuación de la luz en función de la distancia al fragmento, simulando cómo la luz se atenúa en el mundo real
//main: calcula el color final del fragmento, combinando la iluminación difusa, especular y ambiental, y aplicando sombras y atenuación según corresponda.
//IMPORTANTE: Las luces en escena no son estáticas. 
//Otra forma de calcular la iluminación es mediante el uso de mapas de luz (light maps) 
//o mapas de sombras (shadow maps), que son texturas que almacenan información sobre la iluminación y las sombras en la escena.
//suelen ser precalculadas y se utilizan para mejorar el rendimiento y la calidad visual de la iluminación en tiempo real.
//ej: https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping

out vec4 FragColor;//salida del fragmento de color por fragmento 
//fragmento: un fragmento es un fragmento de un píxel que se va a renderizar en la pantalla.

in vec3 FragPos;//posición del fragmento en el espacio del mundo
in vec3 Normal;//normal del fragmento en el espacio del mundo
in vec2 TexCoords;//coordenadas de textura del fragmento
in mat3 TBN;//matriz de transformación de tangente, bitangente y normal para convertir las normales del espacio de la textura al espacio del mundo
in vec4 FragPosLightSpace;//posición del fragmento en el espacio de la luz para calcular sombras

uniform vec3 uDiffuseColor;//color difusa del material, representa el color base del objeto
uniform sampler2D uDiffuseTexture;//textura difusa del material, representa la textura que se aplicará al objeto
uniform sampler2D uNormalTexture;//textura normal del material, representa la textura que se utilizará para simular detalles de superficie
uniform sampler2D uShadowMap;//mapa de sombras generado desde la perspectiva de la luz, utilizado para determinar si un fragmento está en sombra o no
uniform bool uHasDiffuseTexture;//indica si el material tiene una textura difusa o no
uniform bool uHasNormalTexture;//indica si el material tiene una textura normal o no

uniform vec3 viewPos;//posición de la cámara en el espacio del mundo, utilizada para calcular la iluminación especular
uniform mat4 lightSpaceMatrix;
//matriz de transformación de espacio de luz, utilizada para transformar las posiciones de los fragmentos
// al espacio de la luz para calcular sombras, precalculada en el programa principal y pasada al shader
// como uniform dado que no cambia durante la renderización de la escena.

uniform vec3 uLightDir;//dirección de la luz direccional, utilizada para calcular la iluminación difusa y especular
uniform vec3 uLightColor;//color de la luz direccional, utilizada para calcular la iluminación difusa y especular
uniform vec3 uPointLightPositions[4];//posiciones de las luces puntuales, utilizadas para calcular la iluminación difusa y especular
uniform vec3 uPointLightColors[4];//colores de las luces puntuales, utilizadas para calcular la iluminación difusa y especular
uniform int uPointLightCount;//número de luces puntuales activas, utilizado para iterar sobre las luces puntuales y calcular la iluminación difusa y especular
uniform vec3 uSpotLightPos;//posición de la luz puntual, utilizada para calcular la iluminación difusa y especular
uniform vec3 uSpotLightDir;//dirección de la luz puntual, utilizada para calcular la iluminación difusa y especular
uniform vec3 uSpotLightColor;//color de la luz puntual, utilizada para calcular la iluminación difusa y especular
uniform float uSpotInnerCutoff;//ángulo de corte interno de la luz puntual, utilizado para calcular la intensidad de la luz puntual
uniform float uSpotOuterCutoff;//ángulo de corte externo de la luz puntual, utilizado para calcular la intensidad de la luz puntual

//flags de luces activas, utilizados para activar o desactivar la iluminación de cada tipo de luz
uniform float uAmbientStrength;
uniform float uShininess;
uniform bool uEnablePointLight;
uniform bool uEnableSpotLight;
uniform bool uEnableSpecular;
uniform bool uEnableShadows;
uniform bool uEnableAttenuation;
// activa el efecto de ocultamiento ambiental, utilizado para mejorar la calidad de la iluminación
uniform bool uEnableAO;

//función que calcula el factor de sombra para un fragmento dado, utilizando el mapa de sombras generado
// desde la perspectiva de la luz, 
//recibe la posición del fragmento en el espacio de la luz y la normal del fragmento como parámetros
float shadowFactor(vec4 lightSpacePosition, vec3 N)
{
    // Convertir la posición del fragmento en el espacio de la luz a coordenadas de textura
    //o sea 3D a 2D, para poder muestrear el mapa de sombras.
    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w;
    projected = projected * 0.5 + 0.5;// Convertir de [-1, 1] a [0, 1] para las coordenadas de textura.
    // Verificar si la posición proyectada está dentro de los límites del mapa de sombras.
    if (projected.z > 1.0 || projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0) return 0.0;
    // Obtener la profundidad más cercana del mapa de sombras en las coordenadas de textura proyectadas.
    //se coloca en el canal rojo del mapa de sombras, que es un mapa de profundidad.
    //xyz=rgb, w=alpha
    float closestDepth = texture(uShadowMap, projected.xy).r;//x más cercano en el mapa de sombras
    float currentDepth = projected.z;//profundidad actual del fragmento en el espacio de la luz
    //bias: evita shadow acne, autoproyecciones de sombras en la superficie del objeto debido a errores
    // de precisión en la comparación de profundidad.
    // La bias es un valor que se resta a la profundidad actual para evitar que las sombras se muestren 
    // incorrectamente.
    float bias = max(0.005 * (1.0 - dot(N, normalize(uLightDir))), 0.0005);
    float shadow = 0.0;
    //definir el tamaño del texel del mapa de sombras para realizar un muestreo en 
    // 3x3 alrededor del fragmento actual, porque usamos PCF (Percentage Closer Filtering) para 
    //suavizar los bordes de las sombras.

    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += currentDepth - bias > texture(uShadowMap, projected.xy + vec2(x, y) * texelSize).r ? 1.0 : 0.0;
    return shadow / 9.0;
}

//Función que calcula la iluminación especular para un fragmento dado, 
//utilizando el modelo de iluminación de Blinn-Phong.
//Recibe la normal del fragmento, la dirección de la luz y el color de la luz como parámetros.
//normalize: normaliza un vector, es decir, lo convierte en un vector unitario con longitud 1.
vec3 specularLight(vec3 N, vec3 L, vec3 color)
{//V visualmente es la dirección hacia la cámara, H es el vector medio entre L y V
    vec3 V = normalize(viewPos - FragPos);
    vec3 H = normalize(L + V);
    //aplica la ecuación de Blinn-Phong para calcular la intensidad de la luz especular,
    //producto punto: devuelve un valor escalar que representa la proyección de un vector sobre otro.
    //es decir, mide cuánto se parecen dos vectores en términos de dirección.
    //si es negativo, significa que la luz está detrás de la superficie y no contribuye a la iluminación especular.
    //si es 0, significa que la luz es perpendicular a la superficie y no contribuye a la iluminación especular.
    //pero si es positivo, significa que la luz está frente a la superficie y contribuye a la iluminación especular.
    return pow(max(dot(N, H), 0.0), uShininess) * color;
}

//Función que calcula la atenuación de la luz en función de la distancia al fragmento.
//La atenuación es un factor que reduce la intensidad de la luz a medida que se aleja del fragmento.
//Recibe la distancia al fragmento como parámetro y devuelve un valor entre 0 y 1.
float attenuation(float distanceToLight)
{
    // La fórmula de atenuación utilizada es una combinación de atenuación lineal y cuadrática,
    // que se ajusta a la distancia al fragmento para simular cómo la luz se atenúa en el mundo real.
    // La constante 0.09 y 0.032 son valores empíricos que se ajustan para obtener un resultado visualmente agradable.
    
    return 1.0 / (1.0 + 0.09 * distanceToLight + 0.032 * distanceToLight * distanceToLight);
}

//Aquí pasa la magia, el main de fragment es donde se calcula el color final del fragmento, 
//combinando la iluminación difusa, especular y ambiental,
void main()
{
    // Normalizar la normal del fragmento y aplicar la textura normal si está disponible.
    vec3 N = normalize(Normal);
    if (uHasNormalTexture)
    
    //TBN(Tangent, Bitangent, Normal), es una matriz que transforma las normales del espacio de la textura al espacio 
    //del mundo. 2d a 3d, se multiplica la normal de la textura por 2 y se resta 1 para mapearla de [0, 1] a [-1, 1].NDC
        N = normalize(TBN * (texture(uNormalTexture, TexCoords).rgb * 2.0 - 1.0));//NORMAL
    //si el fragmento no está frente a la cámara, se invierte la normal para que apunte hacia la cámara.
    //gl_FrontFacing es una variable incorporada en GLSL que indica si el fragmento actual está frente a la cámara o no.
    if (!gl_FrontFacing) N = -N;

    // Obtener el color difuso del material, ya sea de la textura difusa o del color difuso uniforme.
    vec3 albedo = uHasDiffuseTexture ? texture(uDiffuseTexture, TexCoords).rgb : uDiffuseColor;
    // Calcular la sombra para el fragmento actual, si  está habilitada.
    //shadowFactor devuelve un valor entre 0 y 1, donde 0 completamente iluminado y 1 completamente en sombra.
    
    float shadow = uEnableShadows ? shadowFactor(FragPosLightSpace, N) : 0.0;
    vec3 ambient = uAmbientStrength * uLightColor * albedo;
    if (uEnableAO)
    {
        //Simula AO su fundamento es que si estás expuesto a la luz ambiental, es probable que estés en un área abierta 
        // no con algo encima, valores empíricos, y producto punto entre la normal y el vector (0, 1, 0) 
        //su resultado es 1 si la normal apunta hacia arriba y 0 si apunta hacia abajo, 
        //lo que significa que el fragmento está en una cavidad o debajo de algo.
        float hemisphericalAO = mix(0.45, 1.0, max(dot(N, vec3(0.0, 1.0, 0.0)), 0.0));
        ambient *= hemisphericalAO;
    }

    vec3 result = ambient;
    //calcula la iluminación direccional, puntual y de foco, sumando sus contribuciones al color final del fragmento.
    //si todas existen, se suman al resultado final, si no no aportan nada al resultado final.
    vec3 directional = max(dot(N, normalize(uLightDir)), 0.0) * uLightColor * (1.0 - shadow);
    result += directional * albedo;
    if (uEnableSpecular)    
        result += specularLight(N, normalize(uLightDir), uLightColor) * (1.0 - shadow);

    if (uEnablePointLight)
    {
        for (int i = 0; i < uPointLightCount; ++i)
        {
            vec3 toLight = uPointLightPositions[i] - FragPos;
            float distanceToLight = length(toLight);
            vec3 L = normalize(toLight);
            // Calcula el factor de atenuación, que reduce la intensidad de la luz a medida que se aleja del foco.
            float factor = uEnableAttenuation ? attenuation(distanceToLight) : 1.0;
            result += max(dot(N, L), 0.0) * uPointLightColors[i] * factor * albedo;
            if (uEnableSpecular)
                result += specularLight(N, L, uPointLightColors[i]) * factor;
        }
    }

    if (uEnableSpotLight)
    {
        vec3 toLight = uSpotLightPos - FragPos;
        float distanceToLight = length(toLight);
        vec3 L = normalize(toLight);
        float theta = dot(L, normalize(-uSpotLightDir));
        float intensity = clamp((theta - uSpotOuterCutoff) /
                                (uSpotInnerCutoff - uSpotOuterCutoff), 0.0, 1.0);
        float factor = uEnableAttenuation ? attenuation(distanceToLight) : 1.0;
        result += max(dot(N, L), 0.0) * uSpotLightColor * intensity * factor * albedo;
        if (uEnableSpecular)
            result += specularLight(N, L, uSpotLightColor) * intensity * factor;
    }

    FragColor = vec4(result, 1.0);
}
