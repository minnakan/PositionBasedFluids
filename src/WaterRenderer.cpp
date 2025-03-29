#include "WaterRenderer.h"
#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iomanip> 
#include <cmath>

WaterRenderer::WaterRenderer()
    : screenWidth(0), screenHeight(0), particleRadius(0.0f),
    depthShader(nullptr), normalShader(nullptr), smoothingShader(nullptr), passthroughShader(nullptr),
    particleVAO(0), particleVBO(0), quadVAO(0), quadVBO(0), maxParticles(100000),
    depthFBO(0), depthTexture(0), depthColorTexture(0),
    normalFBO(0), normalTexture(0),
    smoothedDepthFBO(0), smoothedDepthTexture(0)
{
}

WaterRenderer::~WaterRenderer() {
    cleanup();
}

bool WaterRenderer::initialize(int width, int height, float particleRadius) {
    this->screenWidth = width;
    this->screenHeight = height;
    this->particleRadius = particleRadius;

    // Load shaders
    try {
        depthShader = new Shader(RESOURCES_PATH"particle_depth.vert", RESOURCES_PATH"particle_depth.frag");
        normalShader = new Shader(RESOURCES_PATH"quad.vert", RESOURCES_PATH"normal_reconstruction.frag");
        smoothingShader = new Shader(RESOURCES_PATH"quad.vert", RESOURCES_PATH"smoothing.frag");
        passthroughShader = new Shader(RESOURCES_PATH"quad.vert", RESOURCES_PATH"passthrough.frag");
        std::cout << "[WaterRenderer] Shaders loaded successfully" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[WaterRenderer] Failed to load shaders: " << e.what() << std::endl;
        return false;
    }

    // Create VAO for particle rendering
    createParticleVAO();

    // Create VAO for full-screen quad rendering
    createQuadVAO();

    // Create framebuffers
    createFramebuffers(width, height);

    return true;
}

void WaterRenderer::createParticleVAO() {
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

    // Generate indices from 0 to maxParticles-1
    std::vector<GLuint> indices(maxParticles);
    for (unsigned int i = 0; i < maxParticles; i++) {
        indices[i] = i;
    }

    glBufferData(GL_ARRAY_BUFFER, maxParticles * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // Set up attribute for vertex ID (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(GLuint), (void*)0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void WaterRenderer::createQuadVAO() {
    // Create VAO and VBO for a fullscreen quad
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);

    // Vertices for a fullscreen quad
    float quadVertices[] = {
        // positions        // texture coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Position attributes (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    // Texture coordinates (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

void WaterRenderer::createFramebuffers(int width, int height) {
    // Create depth framebuffer
    glGenFramebuffers(1, &depthFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);

    // Create depth texture
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    // We need a color attachment even though we're not writing to it (OpenGL requirement)
    glGenTextures(1, &depthColorTexture);
    glBindTexture(GL_TEXTURE_2D, depthColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthColorTexture, 0);

    // Check framebuffer status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[WaterRenderer] Depth framebuffer is not complete!" << std::endl;
    }

    // Create normal framebuffer
    glGenFramebuffers(1, &normalFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, normalFBO);

    // Create normal texture
    glGenTextures(1, &normalTexture);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, normalTexture, 0);

    // Check framebuffer status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[WaterRenderer] Normal framebuffer is not complete!" << std::endl;
    }

    // Create smoothed depth framebuffer
    glGenFramebuffers(1, &smoothedDepthFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, smoothedDepthFBO);

    // Create smoothed depth texture
    glGenTextures(1, &smoothedDepthTexture);
    glBindTexture(GL_TEXTURE_2D, smoothedDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, smoothedDepthTexture, 0);

    // Check framebuffer status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[WaterRenderer] Smoothed depth framebuffer is not complete!" << std::endl;
    }

    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void WaterRenderer::renderFluid(const PBFSystem& pbf, const Camera& camera, const glm::vec3& lightPos) {
    // Only proceed if the PBF system has a compute system initialized
    if (!pbf.computeSystemInitialized) {
        std::cerr << "[WaterRenderer] Cannot render: compute system not initialized" << std::endl;
        return;
    }

    unsigned int numParticles = pbf.computeSystem->getNumParticles();
    if (numParticles == 0) {
        return;
    }

    // Compute view and projection matrices
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = glm::perspective(
        glm::radians(camera.Zoom),
        (float)screenWidth / (float)screenHeight,
        0.1f, 1000.0f
    );
    glm::mat4 model = glm::mat4(1.0f);

    // Get the particle buffer ID from the compute system
    GLuint particleBufferId = pbf.computeSystem->getParticleBufferId();

    // -----------------------------------------
    // 1. Depth Pass - Render particles to depth buffer
    // -----------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    // Enable depth test and point sprites
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Use depth shader for rendering particles
    depthShader->use();
    depthShader->setMat4("model", model);
    depthShader->setMat4("view", view);
    depthShader->setMat4("projection", projection);
    depthShader->setFloat("particleRadius", particleRadius);

    // Bind particle SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleBufferId);

    // Draw particles
    glBindVertexArray(particleVAO);
    glDrawArrays(GL_POINTS, 0, numParticles);
    glBindVertexArray(0);

    // Run depth buffer analysis occasionally
    static int frameCounter = 0;
    if (frameCounter++ % 120 == 0) {
        analyzeDepthBuffer();
    }

    // -----------------------------------------
    // 2. Normal Reconstruction Pass
    // -----------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, normalFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use normal reconstruction shader
    normalShader->use();
    normalShader->setInt("depthMap", 0);
    normalShader->setMat4("projection", projection);
    normalShader->setMat4("view", view);
    normalShader->setVec2("screenSize", glm::vec2(screenWidth, screenHeight));
    normalShader->setFloat("normalStrength", 0.02f); // Adjust this value based on your scene scale

    // Bind depth texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexture);

    // Render fullscreen quad
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // -----------------------------------------
    // 3. Smoothing Pass - Apply curvature flow
    // -----------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, smoothedDepthFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use smoothing shader
    smoothingShader->use();
    smoothingShader->setInt("depthMap", 0);
    smoothingShader->setInt("normalMap", 1);
    smoothingShader->setVec2("screenSize", glm::vec2(screenWidth, screenHeight));
    smoothingShader->setInt("smoothingIterations", 2);
    smoothingShader->setFloat("smoothingStrength", 0.3f);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTexture);

    // Render fullscreen quad
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // -----------------------------------------
    // 4. Visualize results (for debugging) 
    // -----------------------------------------
    // Choose which buffer to display (for testing)
    // 0 = original depth, 1 = normals, 2 = smoothed depth
    int displayBuffer = 2;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use passthrough shader
    passthroughShader->use();
    passthroughShader->setInt("inputTexture", 0);

    // Bind the chosen texture
    glActiveTexture(GL_TEXTURE0);
    if (displayBuffer == 0) {
        // Display original depth
        glBindTexture(GL_TEXTURE_2D, depthTexture);
    }
    else if (displayBuffer == 1) {
        // Display normals
        glBindTexture(GL_TEXTURE_2D, normalTexture);
    }
    else {
        // Display smoothed depth
        glBindTexture(GL_TEXTURE_2D, smoothedDepthTexture);
    }

    // Render fullscreen quad
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // Cleanup
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glActiveTexture(GL_TEXTURE0);

    // Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[WaterRenderer] OpenGL error: 0x" << std::hex << err << std::dec << std::endl;
    }
}

void WaterRenderer::resize(int width, int height) {
    if (width == screenWidth && height == screenHeight) return;

    screenWidth = width;
    screenHeight = height;

    // Resize framebuffers
    cleanup();
    createFramebuffers(width, height);
}

void WaterRenderer::cleanup() {
    // Delete shaders
    if (depthShader) delete depthShader;
    depthShader = nullptr;

    if (normalShader) delete normalShader;
    normalShader = nullptr;

    if (smoothingShader) delete smoothingShader;
    smoothingShader = nullptr;

    if (passthroughShader) delete passthroughShader;
    passthroughShader = nullptr;

    // Delete VAOs and VBOs
    if (particleVAO) glDeleteVertexArrays(1, &particleVAO);
    if (particleVBO) glDeleteBuffers(1, &particleVBO);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);

    // Delete framebuffers and textures
    if (depthFBO) glDeleteFramebuffers(1, &depthFBO);
    if (depthTexture) glDeleteTextures(1, &depthTexture);
    if (depthColorTexture) glDeleteTextures(1, &depthColorTexture);
    if (normalFBO) glDeleteFramebuffers(1, &normalFBO);
    if (normalTexture) glDeleteTextures(1, &normalTexture);
    if (smoothedDepthFBO) glDeleteFramebuffers(1, &smoothedDepthFBO);
    if (smoothedDepthTexture) glDeleteTextures(1, &smoothedDepthTexture);

    // Reset all identifiers
    particleVAO = 0;
    particleVBO = 0;
    quadVAO = 0;
    quadVBO = 0;
    depthFBO = 0;
    depthTexture = 0;
    depthColorTexture = 0;
    normalFBO = 0;
    normalTexture = 0;
    smoothedDepthFBO = 0;
    smoothedDepthTexture = 0;
}

void WaterRenderer::analyzeDepthBuffer() {
    // Sample the depth buffer at multiple points to understand the range
    const int sampleSize = 100;
    std::vector<float> depthSamples(sampleSize);

    // Read depth values from a grid of points in the center of the screen
    int centerX = screenWidth / 2;
    int centerY = screenHeight / 2;
    int gridSize = 10; // 10x10 grid

    // Make sure we're reading from the depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);

    int sampleIndex = 0;
    for (int y = -gridSize / 2; y < gridSize / 2 && sampleIndex < sampleSize; y++) {
        for (int x = -gridSize / 2; x < gridSize / 2 && sampleIndex < sampleSize; x++) {
            float depthValue;
            int pixelX = centerX + x * 20; // Space samples 20 pixels apart
            int pixelY = centerY + y * 20;

            // Make sure we stay within screen bounds
            if (pixelX >= 0 && pixelX < screenWidth && pixelY >= 0 && pixelY < screenHeight) {
                glReadPixels(pixelX, pixelY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthValue);
                depthSamples[sampleIndex++] = depthValue;
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Find min and max depth values
    float minDepth = 1.0f;
    float maxDepth = 0.0f;
    for (int i = 0; i < sampleIndex; i++) {
        if (depthSamples[i] < minDepth) minDepth = depthSamples[i];
        if (depthSamples[i] > maxDepth) maxDepth = depthSamples[i];
    }

    // Calculate average and standard deviation
    float sum = 0.0f;
    for (int i = 0; i < sampleIndex; i++) {
        sum += depthSamples[i];
    }
    float avgDepth = sum / sampleIndex;

    float variance = 0.0f;
    for (int i = 0; i < sampleIndex; i++) {
        variance += (depthSamples[i] - avgDepth) * (depthSamples[i] - avgDepth);
    }
    variance /= sampleIndex;
    float stdDev = sqrt(variance);

    // Print depth statistics
    std::cout << "Depth Buffer Analysis:" << std::endl;
    std::cout << "  Samples: " << sampleIndex << std::endl;
    std::cout << "  Min depth: " << minDepth << std::endl;
    std::cout << "  Max depth: " << maxDepth << std::endl;
    std::cout << "  Range: " << (maxDepth - minDepth) << std::endl;
    std::cout << "  Average: " << avgDepth << std::endl;
    std::cout << "  Standard deviation: " << stdDev << std::endl;

    // Calculate and print histogram for depth distribution
    const int bins = 10;
    int histogram[bins] = { 0 };

    for (int i = 0; i < sampleIndex; i++) {
        int bin = (int)((depthSamples[i] - minDepth) / (maxDepth - minDepth) * bins);
        if (bin >= bins) bin = bins - 1;
        if (bin < 0) bin = 0;
        histogram[bin]++;
    }

    std::cout << "  Depth distribution:" << std::endl;
    for (int i = 0; i < bins; i++) {
        float binStart = minDepth + (maxDepth - minDepth) * i / bins;
        float binEnd = minDepth + (maxDepth - minDepth) * (i + 1) / bins;
        std::cout << "    " << std::fixed << std::setprecision(4)
            << binStart << " - " << binEnd << ": "
            << histogram[i] << " samples" << std::endl;
    }
}