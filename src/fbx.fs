#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in mat3 TBN;
in vec4 FragPosLightSpace;

uniform vec3 uDiffuseColor;
uniform sampler2D uDiffuseTexture;
uniform sampler2D uNormalTexture;
uniform sampler2D uShadowMap;
uniform bool uHasDiffuseTexture;
uniform bool uHasNormalTexture;

uniform vec3 viewPos;
uniform mat4 lightSpaceMatrix;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uPointLightPositions[4];
uniform vec3 uPointLightColors[4];
uniform int uPointLightCount;
uniform vec3 uSpotLightPos;
uniform vec3 uSpotLightDir;
uniform vec3 uSpotLightColor;
uniform float uSpotInnerCutoff;
uniform float uSpotOuterCutoff;

uniform float uAmbientStrength;
uniform float uShininess;
uniform bool uEnablePointLight;
uniform bool uEnableSpotLight;
uniform bool uEnableSpecular;
uniform bool uEnableShadows;
uniform bool uEnableAttenuation;
uniform bool uEnableAO;

float shadowFactor(vec4 lightSpacePosition, vec3 N)
{
    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w;
    projected = projected * 0.5 + 0.5;
    if (projected.z > 1.0 || projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0) return 0.0;
    float closestDepth = texture(uShadowMap, projected.xy).r;
    float currentDepth = projected.z;
    float bias = max(0.005 * (1.0 - dot(N, normalize(uLightDir))), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += currentDepth - bias > texture(uShadowMap, projected.xy + vec2(x, y) * texelSize).r ? 1.0 : 0.0;
    return shadow / 9.0;
}

vec3 specularLight(vec3 N, vec3 L, vec3 color)
{
    vec3 V = normalize(viewPos - FragPos);
    vec3 H = normalize(L + V);
    return pow(max(dot(N, H), 0.0), uShininess) * color;
}

float attenuation(float distanceToLight)
{
    return 1.0 / (1.0 + 0.09 * distanceToLight + 0.032 * distanceToLight * distanceToLight);
}

void main()
{
    vec3 N = normalize(Normal);
    if (uHasNormalTexture)
        N = normalize(TBN * (texture(uNormalTexture, TexCoords).rgb * 2.0 - 1.0));
    if (!gl_FrontFacing) N = -N;

    vec3 albedo = uHasDiffuseTexture ? texture(uDiffuseTexture, TexCoords).rgb : uDiffuseColor;
    float shadow = uEnableShadows ? shadowFactor(FragPosLightSpace, N) : 0.0;
    vec3 ambient = uAmbientStrength * uLightColor * albedo;
    if (uEnableAO)
    {
        float hemisphericalAO = mix(0.45, 1.0, max(dot(N, vec3(0.0, 1.0, 0.0)), 0.0));
        ambient *= hemisphericalAO;
    }

    vec3 result = ambient;
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
