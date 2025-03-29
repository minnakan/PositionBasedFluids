#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Shader.h"
#include "Camera.h"
#include "PBFSystem.h"

// Forward declaration to avoid circular dependency
class PBFSystem;

class WaterRenderer {
public:
    WaterRenderer();
    ~WaterRenderer();

    bool initialize(int width, int height, float particleRadius);
    void renderFluid(const PBFSystem& pbf, const Camera& camera, const glm::vec3& lightPos);
    void resize(int width, int height);
    void cleanup();

    void analyzeDepthBuffer();

private:
    void createParticleVAO();
    void createQuadVAO();
    void createFramebuffers(int width, int height);

    // Screen dimensions
    int screenWidth;
    int screenHeight;
    float particleRadius;

    // Maximum number of particles
    unsigned int maxParticles;

    // Shaders
    Shader* depthShader;      // For depth rendering
    Shader* normalShader;     // For normal reconstruction
    Shader* passthroughShader; // For visualization

    // Particle rendering
    GLuint particleVAO;
    GLuint particleVBO;

    // Quad rendering (for post-processing)
    GLuint quadVAO;
    GLuint quadVBO;

    // Framebuffers and textures
    GLuint depthFBO;          // Framebuffer for depth pass
    GLuint depthTexture;      // Depth texture
    GLuint depthColorTexture; // Color attachment for depth FBO (required by OpenGL)

    GLuint normalFBO;         // Framebuffer for normal reconstruction
    GLuint normalTexture;     // Normal texture
};