#include "ComputeShader.h"
#include <fstream>
#include <sstream>
#include <iostream>

ComputeShader::ComputeShader(const char* computePath) {
    std::string computeCode;
    std::ifstream cShaderFile;

    std::cout << "Loading compute shader from: " << computePath << std::endl;
    cShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        cShaderFile.open(computePath);
        std::stringstream cShaderStream;

        cShaderStream << cShaderFile.rdbuf();
        cShaderFile.close();
        computeCode = cShaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
        throw std::runtime_error("Failed to read compute shader file");
    }

    const char* cShaderCode = computeCode.c_str();

    unsigned int compute;
    int success;
    char infoLog[512];

    // Compute shader
    compute = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(compute, 1, &cShaderCode, NULL);
    glCompileShader(compute);

    // Check for compile errors
    glGetShaderiv(compute, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(compute, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPUTE::COMPILATION_FAILED\n" << infoLog << std::endl;
        std::cerr << "Shader source code:\n" << computeCode << std::endl;
        throw std::runtime_error("Compute shader compilation failed");
    }

    //Create shader program
    ID = glCreateProgram();
    glAttachShader(ID, compute);
    glLinkProgram(ID);

    //Check for linking errors
    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(ID, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        throw std::runtime_error("Compute shader program linking failed");
    }

    //Print active uniforms
    GLint numUniforms = 0;
    glGetProgramiv(ID, GL_ACTIVE_UNIFORMS, &numUniforms);
    std::cout << "Compute shader has " << numUniforms << " active uniforms.\n";

    GLchar name[128];
    GLint size;
    GLenum type;
    GLsizei length;

    //Print shader program validation status
    glValidateProgram(ID);
    glGetProgramiv(ID, GL_VALIDATE_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(ID, 512, NULL, infoLog);
        std::cerr << "WARNING::SHADER::PROGRAM::VALIDATION_FAILED\n" << infoLog << std::endl;
    }

    //Delete shader as it's linked into the program now
    glDeleteShader(compute);

    std::cout << "Compute shader (ID=" << ID << ") compilation and linking successful.\n";

    // Verify the program is valid
    if (glIsProgram(ID) == GL_FALSE) {
        std::cerr << "ERROR: Created shader ID is not a valid program object!\n";
        throw std::runtime_error("Invalid shader program object");
    }
}

ComputeShader::~ComputeShader() {
    glDeleteProgram(ID);
}

void ComputeShader::use() {
    // Check for any errors before using
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error before using compute shader: 0x"<< std::hex << err << std::dec << std::endl;
    }

    glUseProgram(ID);

    // Verify shader is actually active
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

    if (currentProgram != ID) {
        std::cerr << "ERROR: Failed to activate compute shader (active ID="<< currentProgram << ", expected ID=" << ID << ")" << std::endl;
    }

    // Check for errors after activation
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after using compute shader: 0x"<< std::hex << err << std::dec << std::endl;
    }
}

void ComputeShader::dispatch(unsigned int numGroupsX, unsigned int numGroupsY, unsigned int numGroupsZ) {
    //Verify shader is active
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

    if (currentProgram != ID) {
        std::cerr << "ERROR: Cannot dispatch - wrong shader active (active ID="<< currentProgram << ", compute shader ID=" << ID << ")" << std::endl;
        return;
    }

    glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);

    //Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after dispatch: 0x"<< std::hex << err << std::dec << std::endl;
    }
}

void ComputeShader::wait() {
    // Issue a memory barrier to ensure compute shader completes before subsequent operations
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
}

void ComputeShader::setBool(const std::string& name, bool value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void ComputeShader::setInt(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void ComputeShader::setFloat(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void ComputeShader::setVec3(const std::string& name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
}

void ComputeShader::setMat4(const std::string& name, const glm::mat4& mat) const {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}