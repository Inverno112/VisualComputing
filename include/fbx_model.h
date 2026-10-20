#pragma once
// Cargador minimo de FBX (geometria estatica) con Assimp.
// Vertex layout: pos(3) + normal(3). Color difuso por malla (uniform en shader).

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <learnopengl/shader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct GpuMesh
{
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLuint diffuseTexture = 0, normalTexture = 0;
    GLsizei indexCount = 0;
    glm::vec3 diffuse = glm::vec3(0.8f);
    bool hasDiffuseTexture = false;
    bool hasNormalTexture = false;
};

class FbxModel
{
public:
    std::vector<GpuMesh> meshes;

    ~FbxModel()
    {
        for (auto& m : meshes)
        {
            glDeleteVertexArrays(1, &m.vao);
            glDeleteBuffers(1, &m.vbo);
            glDeleteBuffers(1, &m.ebo);
            glDeleteTextures(1, &m.diffuseTexture);
            glDeleteTextures(1, &m.normalTexture);
        }
    }

    bool load(const std::string& path)
    {
        Assimp::Importer importer;
        // PreTransformVertices: aplica la jerarquia de nodos a los vertices (modelo estatico).
        // Descarta animaciones/huesos; para skinning habria que quitarlo.
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_JoinIdenticalVertices |
            aiProcess_CalcTangentSpace |
            aiProcess_PreTransformVertices);

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        {
            std::cout << "Assimp error: " << importer.GetErrorString() << std::endl;
            return false;
        }

        struct Cpu
        {
            std::vector<float> v;
            std::vector<unsigned> idx;
            glm::vec3 color;
            std::string diffusePath;
            std::string normalPath;
        };
        std::vector<Cpu> cpu;
        const std::filesystem::path modelDirectory = std::filesystem::path(path).parent_path();

        glm::vec3 bmin(std::numeric_limits<float>::max());
        glm::vec3 bmax(std::numeric_limits<float>::lowest());

        for (unsigned i = 0; i < scene->mNumMeshes; ++i)
        {
            const aiMesh* am = scene->mMeshes[i];
            if (!am->HasNormals()) continue;

            Cpu c;
            c.color = glm::vec3(0.8f);
            aiColor3D col;
            if (scene->mMaterials[am->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, col) == AI_SUCCESS)
                c.color = glm::vec3(col.r, col.g, col.b);

            const aiMaterial* material = scene->mMaterials[am->mMaterialIndex];
            c.diffusePath = texturePath(scene, material, aiTextureType_DIFFUSE, modelDirectory);
            if (c.diffusePath.empty())
                c.diffusePath = texturePath(scene, material, aiTextureType_BASE_COLOR, modelDirectory);
            c.normalPath = texturePath(scene, material, aiTextureType_NORMALS, modelDirectory);
            if (c.normalPath.empty())
                c.normalPath = texturePath(scene, material, aiTextureType_HEIGHT, modelDirectory);
            if (c.normalPath.empty())
                c.normalPath = texturePath(scene, material, aiTextureType_NORMAL_CAMERA, modelDirectory);

            c.v.reserve(am->mNumVertices * 14);
            for (unsigned j = 0; j < am->mNumVertices; ++j)
            {
                glm::vec3 p(am->mVertices[j].x, am->mVertices[j].y, am->mVertices[j].z);
                glm::vec3 n(am->mNormals[j].x, am->mNormals[j].y, am->mNormals[j].z);
                glm::vec3 t = am->HasTangentsAndBitangents()
                    ? glm::vec3(am->mTangents[j].x, am->mTangents[j].y, am->mTangents[j].z)
                    : glm::vec3(1.0f, 0.0f, 0.0f);
                glm::vec3 b = am->HasTangentsAndBitangents()
                    ? glm::vec3(am->mBitangents[j].x, am->mBitangents[j].y, am->mBitangents[j].z)
                    : glm::cross(n, t);
                aiVector3D uv = am->HasTextureCoords(0) ? am->mTextureCoords[0][j] : aiVector3D(0.0f, 0.0f, 0.0f);
                bmin = glm::min(bmin, p);
                bmax = glm::max(bmax, p);
                c.v.insert(c.v.end(), { p.x, p.y, p.z,
                                        n.x, n.y, n.z, uv.x, uv.y,
                                        t.x, t.y, t.z, b.x, b.y, b.z });
            }
            for (unsigned f = 0; f < am->mNumFaces; ++f)
            {
                if (am->mFaces[f].mNumIndices != 3) continue;
                for (unsigned k = 0; k < 3; ++k) c.idx.push_back(am->mFaces[f].mIndices[k]);
            }
            cpu.push_back(std::move(c));
        }

        if (cpu.empty()) { std::cout << "El FBX no tiene mallas validas\n"; return false; }

        // Normaliza: centra en el origen y escala a ~2 unidades (los FBX vienen en cm, m, etc.)
        glm::vec3 center = 0.5f * (bmin + bmax);
        glm::vec3 size = bmax - bmin;
        float scale = 2.0f / std::max(size.x, std::max(size.y, size.z));
        boundsMin = (bmin - center) * scale;
        boundsMax = (bmax - center) * scale;

        for (auto& c : cpu)
        {
            for (size_t i = 0; i < c.v.size(); i += 14)
            {
                c.v[i + 0] = (c.v[i + 0] - center.x) * scale;
                c.v[i + 1] = (c.v[i + 1] - center.y) * scale;
                c.v[i + 2] = (c.v[i + 2] - center.z) * scale;
            }

            GpuMesh g;
            g.diffuse = c.color;
            g.indexCount = (GLsizei)c.idx.size();
            g.diffuseTexture = loadTexture(scene, c.diffusePath, glm::vec3(1.0f));
            g.normalTexture = loadTexture(scene, c.normalPath, glm::vec3(0.5f, 0.5f, 1.0f));
            g.hasDiffuseTexture = !c.diffusePath.empty();
            g.hasNormalTexture = !c.normalPath.empty();
            glGenVertexArrays(1, &g.vao);
            glGenBuffers(1, &g.vbo);
            glGenBuffers(1, &g.ebo);
            glBindVertexArray(g.vao);
            glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
            glBufferData(GL_ARRAY_BUFFER, c.v.size() * sizeof(float), c.v.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, c.idx.size() * sizeof(unsigned), c.idx.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            constexpr GLsizei stride = 14 * sizeof(float);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)(11 * sizeof(float)));
            glBindVertexArray(0);
            meshes.push_back(g);
        }
        return true;
    }

    void draw(Shader& shader) const
    {
        for (const auto& m : meshes)
        {
            shader.setVec3("uDiffuseColor", m.diffuse);
            shader.setBool("uHasDiffuseTexture", m.hasDiffuseTexture);
            shader.setBool("uHasNormalTexture", m.hasNormalTexture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m.diffuseTexture);
            shader.setInt("uDiffuseTexture", 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m.normalTexture);
            shader.setInt("uNormalTexture", 1);
            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }

    void drawDepth() const
    {
        for (const auto& m : meshes)
        {
            glBindVertexArray(m.vao);
            glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }

    glm::vec3 boundsMin = glm::vec3(0.0f), boundsMax = glm::vec3(0.0f);

private:
    static std::string texturePath(const aiScene* scene, const aiMaterial* material,
                                    aiTextureType type, const std::filesystem::path& directory)
    {
        if (material->GetTextureCount(type) == 0) return {};
        aiString name;
        if (material->GetTexture(type, 0, &name) != AI_SUCCESS) return {};
        std::string textureName = name.C_Str();
        if (textureName.empty()) return {};
        std::replace(textureName.begin(), textureName.end(), '\\', '/');
        if (textureName[0] == '*' && scene->GetEmbeddedTexture(textureName.c_str()))
            return textureName;
        return (directory / textureName).lexically_normal().string();
    }

    static GLuint loadTexture(const aiScene* scene, const std::string& path, const glm::vec3& fallback)
    {
        stbi_set_flip_vertically_on_load(true);
        int width = 1, height = 1, channels = 4;
        unsigned char fallbackPixels[4] = {
            static_cast<unsigned char>(fallback.r * 255.0f),
            static_cast<unsigned char>(fallback.g * 255.0f),
            static_cast<unsigned char>(fallback.b * 255.0f), 255
        };
        unsigned char* pixels = fallbackPixels;
        bool ownedPixels = false;
        if (!path.empty() && path[0] == '*')
        {
            const aiTexture* embedded = scene->GetEmbeddedTexture(path.c_str());
            if (embedded && embedded->mHeight == 0)
                pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(embedded->pcData),
                                               static_cast<int>(embedded->mWidth), &width, &height, &channels, 4);
            if (pixels != nullptr) ownedPixels = true;
            else pixels = fallbackPixels;
        }
        else if (!path.empty())
        {
            pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
            if (pixels != nullptr) ownedPixels = true;
        }

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        if (ownedPixels) stbi_image_free(pixels);
        if (!path.empty() && pixels == fallbackPixels)
            std::cout << "No se pudo cargar la textura: " << path << std::endl;
        return texture;
    }
};
