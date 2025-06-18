#define GLM_ENABLE_EXPERIMENTAL
#include "WaterRenderer.h"
#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Shader.h"
#include "ComputeShader.h"
#include "PBFSystem.h"

WaterRenderer::WaterRenderer()
    : screenWidth(0), screenHeight(0), particleRadius(0.0f),
    surfaceDetectionShader(nullptr), smoothCenterShader(nullptr), anisotropyShader(nullptr),
    particleShader(nullptr), surfaceShader(nullptr),
    particleVAO(0), particleVBO(0),
    surfaceVAO(0), surfaceVBO(0), surfaceEBO(0),
    surfaceVertexCount(0), surfaceIndexCount(0),
    maxParticles(1000000),
    surfaceParticleBuffer(0), smoothedCentersBuffer(0), anisotropyBuffer(0),
    renderMode(RenderMode::ANISOTROPIC_PARTICLES){
}

WaterRenderer::~WaterRenderer() {
    cleanup();
}

bool WaterRenderer::initialize(int width, int height, float particleRadius) {
    this->screenWidth = width;
    this->screenHeight = height;
    this->particleRadius = particleRadius;

    try {
        // Load compute shaders
        surfaceDetectionShader = new ComputeShader(RESOURCES_PATH"surface_detection.comp");
        smoothCenterShader = new ComputeShader(RESOURCES_PATH"smooth_centers.comp");
        anisotropyShader = new ComputeShader(RESOURCES_PATH"anisotropy.comp");

        // Log shader loading status
        std::cout << "[DEBUG] Loaded shaders - IDs: surface="
            << (surfaceDetectionShader ? surfaceDetectionShader->ID : 0)
            << ", smooth=" << (smoothCenterShader ? smoothCenterShader->ID : 0)
            << ", anisotropy=" << (anisotropyShader ? anisotropyShader->ID : 0) << std::endl;

        // Load rendering shaders
        particleShader = new Shader(RESOURCES_PATH"anisotropic_particle.vert", RESOURCES_PATH"anisotropic_particle.frag");
        surfaceShader = new Shader(RESOURCES_PATH"surface.vert", RESOURCES_PATH"surface.frag");
    }
    catch (const std::exception& e) {
        std::cerr << "[WaterRenderer] Shader initialization error: " << e.what() << std::endl;
        return false;
    }

    createParticleVAO();
    //createSurfaceVAO();

    // Create shader storage buffers
    glGenBuffers(1, &surfaceParticleBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, surfaceParticleBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxParticles * sizeof(GLint), nullptr, GL_DYNAMIC_COPY);
    std::cout << "[DEBUG] Created surfaceParticleBuffer ID: " << surfaceParticleBuffer << std::endl;

    glGenBuffers(1, &smoothedCentersBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, smoothedCentersBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxParticles * sizeof(glm::vec4), nullptr, GL_DYNAMIC_COPY);
    std::cout << "[DEBUG] Created smoothedCentersBuffer ID: " << smoothedCentersBuffer << std::endl;

    glGenBuffers(1, &anisotropyBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, anisotropyBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxParticles * sizeof(glm::mat4), nullptr, GL_DYNAMIC_COPY);
    std::cout << "[DEBUG] Created anisotropyBuffer ID: " << anisotropyBuffer << std::endl;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    float cellSize = particleRadius * 2.5f;
    return true;
}

void WaterRenderer::renderFluid(const PBFSystem& pbf, const Camera& camera, const glm::vec3& lightPos) {
    if (!pbf.computeSystemInitialized) {
        std::cerr << "[WaterRenderer] Cannot render: compute system not initialized" << std::endl;
        return;
    }

    unsigned int numParticles = pbf.computeSystem->getNumParticles();
    if (numParticles == 0) return;

    // For anisotropic particles mode
    if (renderMode == RenderMode::ANISOTROPIC_PARTICLES ||
        renderMode == RenderMode::PARTICLES_AND_SURFACE) {

        // First compute the anisotropic parameters
        computeAnisotropicParameters(pbf);

        // Then render the particles using the anisotropic shader
        renderAnisotropicParticles(pbf, camera, lightPos);

    }
}

void WaterRenderer::computeAnisotropicParameters(const PBFSystem& pbf) {
    GLuint particleBufferId = pbf.computeSystem->getParticleBufferId();
    unsigned int numParticles = pbf.computeSystem->getNumParticles();

    if (numParticles == 0) {
        std::cerr << "[WaterRenderer] No particles to process" << std::endl;
        return;
    }

    std::cout << "[DEBUG] Computing anisotropic parameters for " << numParticles << " particles" << std::endl;
    std::cout << "[DEBUG] Buffer IDs: particle=" << particleBufferId
        << ", surface=" << surfaceParticleBuffer
        << ", centers=" << smoothedCentersBuffer
        << ", anisotropy=" << anisotropyBuffer << std::endl;

    // Initialize all buffers with proper data to avoid undefined behavior
    // Surface flags buffer (int)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, surfaceParticleBuffer);
    std::vector<GLint> initialFlags(numParticles, 0);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(GLint), initialFlags.data(), GL_DYNAMIC_COPY);

    // Smoothed centers buffer (vec4)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, smoothedCentersBuffer);
    std::vector<glm::vec4> initialCenters(numParticles, glm::vec4(0.0f));
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(glm::vec4), initialCenters.data(), GL_DYNAMIC_COPY);

    // Anisotropy buffer (mat4)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, anisotropyBuffer);
    std::vector<glm::mat4> initialMatrices(numParticles, glm::mat4(1.0f));
    glBufferData(GL_SHADER_STORAGE_BUFFER, numParticles * sizeof(glm::mat4), initialMatrices.data(), GL_DYNAMIC_COPY);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // STEP 1: Surface Detection
    if (!surfaceDetectionShader) {
        std::cerr << "[ERROR] Surface detection shader is null!" << std::endl;
        return;
    }

    surfaceDetectionShader->use();

    // Get uniform locations for debugging
    GLuint shaderProgram = surfaceDetectionShader->ID;
    GLint numParticlesLoc = glGetUniformLocation(shaderProgram, "numParticles");
    GLint neighborRadiusLoc = glGetUniformLocation(shaderProgram, "neighborRadius");
    GLint neighborThresholdLoc = glGetUniformLocation(shaderProgram, "neighborThreshold");

    std::cout << "[DEBUG] Surface Detection: numParticles location=" << numParticlesLoc
        << ", neighborRadius location=" << neighborRadiusLoc
        << ", neighborThreshold location=" << neighborThresholdLoc << std::endl;

    // Use direct uniform setting for uint type
    if (numParticlesLoc != -1) {
        // Use glUniform1ui for uint type
        glUniform1ui(numParticlesLoc, numParticles);
    }

    // Set other uniforms
    surfaceDetectionShader->setFloat("neighborRadius", particleRadius * 2.0f);
    surfaceDetectionShader->setInt("neighborThreshold", 25);

    // Bind buffers with explicit binding points
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBufferId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, surfaceParticleBuffer);

    // Check for errors before dispatch
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[ERROR] OpenGL error before surface detection: 0x" << std::hex << err << std::dec << std::endl;
    }

    // Dispatch compute shader
    int workGroupSize = (numParticles + 255) / 256;
    std::cout << "[DEBUG] Dispatching surface detection: " << workGroupSize << " work groups" << std::endl;
    glDispatchCompute(workGroupSize, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Check for errors after dispatch
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[ERROR] OpenGL error after surface detection: 0x" << std::hex << err << std::dec << std::endl;
    }

    // STEP 2: Laplacian Smoothing
    if (!smoothCenterShader) {
        std::cerr << "[ERROR] Smooth center shader is null!" << std::endl;
        return;
    }

    smoothCenterShader->use();

    // Get uniform locations for debugging
    shaderProgram = smoothCenterShader->ID;
    numParticlesLoc = glGetUniformLocation(shaderProgram, "numParticles");
    GLint smoothingRadiusLoc = glGetUniformLocation(shaderProgram, "smoothingRadius");
    GLint lambdaLoc = glGetUniformLocation(shaderProgram, "lambda");

    std::cout << "[DEBUG] Smooth Centers: numParticles location=" << numParticlesLoc
        << ", smoothingRadius location=" << smoothingRadiusLoc
        << ", lambda location=" << lambdaLoc << std::endl;

    // Use direct uniform setting for uint type
    if (numParticlesLoc != -1) {
        // Use glUniform1ui for uint type
        glUniform1ui(numParticlesLoc, numParticles);
    }

    // Set other uniforms
    smoothCenterShader->setFloat("smoothingRadius", particleRadius * 2.5f);
    smoothCenterShader->setFloat("lambda", 0.9f);

    // Bind buffers with explicit binding points
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBufferId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, surfaceParticleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, smoothedCentersBuffer);

    // Check for errors before dispatch
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[ERROR] OpenGL error before center smoothing: 0x" << std::hex << err << std::dec << std::endl;
    }

    // Dispatch compute shader
    std::cout << "[DEBUG] Dispatching smooth centers: " << workGroupSize << " work groups" << std::endl;
    glDispatchCompute(workGroupSize, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Check for errors after dispatch
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[ERROR] OpenGL error after center smoothing: 0x" << std::hex << err << std::dec << std::endl;
    }

    // STEP 3: Anisotropy Calculation
    if (!anisotropyShader) {
        std::cerr << "[ERROR] Anisotropy shader is null!" << std::endl;
        return;
    }

    anisotropyShader->use();

    // Get uniform locations for debugging
    shaderProgram = anisotropyShader->ID;
    numParticlesLoc = glGetUniformLocation(shaderProgram, "numParticles");
    smoothingRadiusLoc = glGetUniformLocation(shaderProgram, "smoothingRadius");
    GLint krLoc = glGetUniformLocation(shaderProgram, "kr");
    GLint ksLoc = glGetUniformLocation(shaderProgram, "ks");

    std::cout << "[DEBUG] Anisotropy: numParticles location=" << numParticlesLoc
        << ", smoothingRadius location=" << smoothingRadiusLoc
        << ", kr location=" << krLoc
        << ", ks location=" << ksLoc << std::endl;

    // Use direct uniform setting for uint type
    if (numParticlesLoc != -1) {
        // Use glUniform1ui for uint type
        glUniform1ui(numParticlesLoc, numParticles);
    }

    // Set other uniforms
    anisotropyShader->setFloat("smoothingRadius", particleRadius * 2.5f);
    anisotropyShader->setFloat("particleRadius", particleRadius);
    anisotropyShader->setFloat("kr", 4.0f);
    anisotropyShader->setFloat("ks", 1400.0f);
    anisotropyShader->setFloat("kn", 0.5f);
    anisotropyShader->setInt("Neps", 25);

    // Bind buffers with explicit binding points
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBufferId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, surfaceParticleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, smoothedCentersBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, anisotropyBuffer);

    // Check buffer bindings
    GLint binding0 = 0, binding1 = 0, binding2 = 0, binding3 = 0;
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 0, &binding0);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 1, &binding1);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 2, &binding2);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 3, &binding3);

    std::cout << "[DEBUG] SSBO bindings: [0]=" << binding0
        << ", [1]=" << binding1 << ", [2]=" << binding2
        << ", [3]=" << binding3 << std::endl;

    // Check for errors before dispatch
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[ERROR] OpenGL error before anisotropy calculation: 0x" << std::hex << err << std::dec << std::endl;
    }

    // Dispatch compute shader
    std::cout << "[DEBUG] Dispatching anisotropy: " << workGroupSize << " work groups" << std::endl;
    glDispatchCompute(workGroupSize, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Check for errors after dispatch
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[ERROR] OpenGL error after anisotropy calculation: 0x" << std::hex << err << std::dec << std::endl;
    }
}

void WaterRenderer::renderAnisotropicParticles(const PBFSystem& pbf, const Camera& camera, const glm::vec3& lightPos) {
    GLuint particleBufferId = pbf.computeSystem->getParticleBufferId();
    unsigned int numParticles = pbf.computeSystem->getNumParticles();

    if (numParticles == 0) {
        std::cerr << "[WaterRenderer] No particles to render" << std::endl;
        return;
    }

    // Basic OpenGL setup
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Use the particle shader
    if (particleShader) {
        particleShader->use();

        // Set shader uniforms
        particleShader->setMat4("view", camera.GetViewMatrix());
        particleShader->setMat4("projection", glm::perspective(glm::radians(camera.Zoom), (float)screenWidth / screenHeight, 0.1f, 1000.0f));
        particleShader->setMat4("model", glm::mat4(1.0f));
        particleShader->setVec3("lightPos", lightPos.x, lightPos.y, lightPos.z);
        particleShader->setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
        particleShader->setFloat("particleRadius", particleRadius);

        // Bind the necessary buffers for anisotropic rendering
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBufferId);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, smoothedCentersBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, anisotropyBuffer);

        // Render the particles
        glBindVertexArray(particleVAO);
        glDrawArrays(GL_POINTS, 0, numParticles);
        glBindVertexArray(0);

        // Unbind buffers
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    }
    else {
        std::cerr << "[WaterRenderer] Anisotropic shader not available!" << std::endl;
    }
}

// Other methods remain unchanged...
void WaterRenderer::createParticleVAO() {
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

    std::vector<GLuint> indices(maxParticles);
    for (unsigned int i = 0; i < maxParticles; i++) indices[i] = i;
    glBufferData(GL_ARRAY_BUFFER, maxParticles * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(GLuint), (void*)0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void WaterRenderer::renderSurface(const Camera& camera, const glm::vec3& lightPos) {
    if (surfaceIndexCount == 0) return;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    surfaceShader->use();
    surfaceShader->setMat4("view", camera.GetViewMatrix());
    surfaceShader->setMat4("projection", glm::perspective(glm::radians(camera.Zoom), (float)screenWidth / screenHeight, 0.1f, 1000.0f));
    surfaceShader->setMat4("model", glm::mat4(1.0f));
    surfaceShader->setVec3("lightPos", lightPos);
    surfaceShader->setVec3("viewPos", camera.Position);
    surfaceShader->setVec3("waterColor", glm::vec3(0.2f, 0.4f, 0.8f));
    surfaceShader->setFloat("ambient", 0.2f);
    surfaceShader->setFloat("specular", 0.7f);
    surfaceShader->setFloat("shininess", 64.0f);

    glBindVertexArray(surfaceVAO);
    glDrawElements(GL_TRIANGLES, surfaceIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void WaterRenderer::cleanup() {
    if (particleVAO) glDeleteVertexArrays(1, &particleVAO);
    if (particleVBO) glDeleteBuffers(1, &particleVBO);
    if (surfaceVAO) glDeleteVertexArrays(1, &surfaceVAO);
    if (surfaceVBO) glDeleteBuffers(1, &surfaceVBO);
    if (surfaceEBO) glDeleteBuffers(1, &surfaceEBO);
    if (surfaceParticleBuffer) glDeleteBuffers(1, &surfaceParticleBuffer);
    if (smoothedCentersBuffer) glDeleteBuffers(1, &smoothedCentersBuffer);
    if (anisotropyBuffer) glDeleteBuffers(1, &anisotropyBuffer);
}