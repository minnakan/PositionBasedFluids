#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class ComputeShader {
public:
    // Program ID
    unsigned int ID;

    ComputeShader(const char* computePath);

    ~ComputeShader();

    void use();

    void dispatch(unsigned int numGroupsX, unsigned int numGroupsY = 1, unsigned int numGroupsZ = 1);

    void wait();

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;
};