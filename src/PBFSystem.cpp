#include "PBFSystem.h"
#include "PBFComputeSystem.h"
#include "Shader.h"
#include <glad/glad.h>
#include <iostream>
#include <random>
#include <glm/gtc/type_ptr.hpp>

PBFSystem::PBFSystem()
{
	//default simulation parameters - Should be set before initScene()
    dt = 0.016f;
    gravity = glm::vec4(0.0f, -9.81f * 1.0f, 0.0f, 0.0f);
    particleRadius = 0.2f;
    h = particleRadius * 2.5f;

    minBoundary = glm::vec4(-8.0f, 0.0f, -10.0f, 0.0f);
    maxBoundary = glm::vec4(8.0f, 100.0f, 10.0f, 0.0f);

    originalMinBoundary = minBoundary;

	cellSize = h;
	maxParticlesPerCell = 64;

    restDensity = 150.0f;

    vorticityEpsilon = 0.008f;
    xsphViscosityCoeff = 0.01f;

    computeSystem = nullptr;
    computeSystemInitialized = false;

    frameCount = 0;
    warmupFrames = 0;

    currentScene = SceneType::DamBreak;

    waveModeActive = false;
    waveTime = 0.0f;
    waveAmplitude = 4.0f;
    waveFrequency = 0.6f;
}

PBFSystem::~PBFSystem()
{
    delete computeSystem;
}

void PBFSystem::initScene(SceneType sceneType)
{
    waveModeActive = false;
    minBoundary.z = originalMinBoundary.z;
    waveTime = 0.0f;

    // For DropBlock, handle differently than other scenes
    if (sceneType == SceneType::DropBlock) {
        // Download current particles from GPU to ensure CPU has current state
        if (computeSystemInitialized) {
            computeSystem->downloadParticles(particles);
        }

        // Store the number of particles before adding the block
        size_t oldParticleCount = particles.size();

        // Add the water block to existing particles
        dropWaterBlock();

        // Debug output to verify particles were added
        std::cout << "[PBFSystem] Added " << (particles.size() - oldParticleCount)
            << " particles. Total now: " << particles.size() << std::endl;

        // Upload the updated particles back to GPU
        if (computeSystemInitialized) {
            computeSystem->uploadParticles(particles);

            // Force re-initialization of GPU rendering with the new particle count
            gpuRenderVAO = 0; // This will trigger a re-initialization in renderParticlesGPU
        }

        // Update current scene AFTER processing
        currentScene = sceneType;
        return; // Return early, don't go through the normal init process
    }

    // For other scenes, continue with normal initialization
    currentScene = sceneType;

    switch (sceneType) {
    case SceneType::DamBreak:
        frameCount = 0;
        particles.clear();
        createDamBreakScene();
        break;
    case SceneType::WaterContainer:
        frameCount = 0;
        particles.clear();
        createWaterContainerScene();
        break;
        // DropBlock case is handled above
    default:
        std::cerr << "[PBFSystem] Unknown scene type, defaulting to dam break\n";
        frameCount = 0;
        particles.clear();
        createDamBreakScene();
        break;
    }

    //Init GPU system
    if (!computeSystemInitialized) {
        initializeComputeSystem();
    }

    if (computeSystemInitialized) {
        computeSystem->uploadParticles(particles);
    }
}

void PBFSystem::step()
{
    if (!computeSystemInitialized) {
        std::cerr << "[PBFSystem] ERROR: compute system not initialized!\n";
        return;
    }


    if (waveModeActive) {
        waveTime += dt;
        float zDisplacement = waveAmplitude * std::sin(2.0f * 3.14159f * waveFrequency * waveTime);
        zDisplacement = std::max(0.0f, zDisplacement);
        minBoundary.z = originalMinBoundary.z + zDisplacement;
    }

    const int numSubsteps = 1;
    const float subDt = dt / numSubsteps;
    float warmupProgress = std::min(1.0f, frameCount / (float)warmupFrames);
    glm::vec4 scaledGravity = gravity * warmupProgress;

    for (int subStep = 0; subStep < numSubsteps; ++subStep) {
        computeSystem->setFrameCount(frameCount);
        computeSystem->updateSimulationParams(dt, scaledGravity, particleRadius, h, minBoundary, maxBoundary, cellSize, maxParticlesPerCell, restDensity,vorticityEpsilon,xsphViscosityCoeff);
        computeSystem->step();
    }

	////Density logging
 //   std::string sceneStr;
 //   switch (currentScene) {
 //   case SceneType::DamBreak: sceneStr = "dambreak"; break;
 //   case SceneType::WaterContainer: sceneStr = "container"; break;
 //   //case SceneType::DropBlock: sceneStr = sceneStr; break;
 //   default: sceneStr = "unknown"; break;
 //   }

 //   std::string filename = "density_" + sceneStr + ".csv";
 //   computeSystem->recordDensityStatistics(filename);



    //computeSystem->downloadParticles(particles);

    frameCount++;
}

void PBFSystem::initializeComputeSystem()
{
    if (!computeSystem) {
        computeSystem = new PBFComputeSystem();
    }

    // Some max capacity
    const unsigned int MAX_PARTICLES = 1000000;
    bool success = computeSystem->initialize(MAX_PARTICLES,dt,gravity,particleRadius,h,minBoundary,maxBoundary,cellSize,maxParticlesPerCell,restDensity, vorticityEpsilon, xsphViscosityCoeff);

    if (success) {
        computeSystemInitialized = true;
        std::cout << "[PBFSystem] GPU compute system initialized\n";
        computeSystem->updateSimulationParams(dt, gravity, particleRadius, h, minBoundary, maxBoundary, cellSize, maxParticlesPerCell,restDensity, vorticityEpsilon, xsphViscosityCoeff);
    }
    else {
        std::cerr << "[PBFSystem] Failed to initialize GPU compute system\n";
    }
}

void PBFSystem::toggleWaveMode()
{
    waveModeActive = !waveModeActive;

    if (waveModeActive) {
        std::cout << "[PBFSystem] Wave mode activated\n";
        waveTime = 0.0f;
    }
    else {
        std::cout << "[PBFSystem] Wave mode deactivated\n";
        minBoundary.z = originalMinBoundary.z;

        if (computeSystemInitialized) {
            computeSystem->updateSimulationParams(dt, gravity, particleRadius, h, minBoundary, maxBoundary, cellSize, maxParticlesPerCell, restDensity, vorticityEpsilon, xsphViscosityCoeff);
        }
    }
}

void PBFSystem::createDamBreakScene()
{
    // Dam break parameters
    const float damWidth = 14.0f;
    const float damHeight = 60.0f;
    const float damDepth = 10.0f;

    const float leftOffset = minBoundary.x + particleRadius * 3.0f;
    const float spacing = particleRadius * 2.1f;

    const int numX = static_cast<int>(damWidth / spacing);
    const int numY = static_cast<int>(damHeight / spacing);
    const int numZ = static_cast<int>(damDepth / spacing);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.001f, 0.001f);

    for (int x = 0; x < numX; ++x) {
        for (int y = 0; y < numY; ++y) {
            for (int z = 0; z < numZ; ++z) {
                Particle p;

                p.position = glm::vec3(leftOffset + x * spacing + jitter(gen) * spacing * 0.01f,minBoundary.y + particleRadius * 2.0f + y * spacing + jitter(gen) * spacing * 0.01f,minBoundary.z + particleRadius * 3.0f + z * spacing + jitter(gen) * spacing * 0.01f);

				//boundary constraints
                p.position.x = std::clamp(p.position.x, minBoundary.x + particleRadius * 1.5f, maxBoundary.x - particleRadius * 1.5f);
                p.position.y = std::clamp(p.position.y, minBoundary.y + particleRadius * 1.5f, maxBoundary.y - particleRadius * 1.5f);
                p.position.z = std::clamp(p.position.z, minBoundary.z + particleRadius * 1.5f, maxBoundary.z - particleRadius * 1.5f);

                p.padding1 = 0.0f;
                p.velocity = glm::vec3(0.0f);
                p.padding2 = 0.0f;
                p.predictedPosition = p.position;
                p.padding3 = 0.0f;

                float heightRatio = static_cast<float>(y) / numY;
                p.color = glm::vec3(heightRatio, 0.2f, 1.0f - heightRatio);
                p.padding4 = 0.0f;
                particles.push_back(p);
            }
        }
    }

    std::cout << "[PBFSystem] Created " << particles.size() << " particles for dam break scene\n";
}

void PBFSystem::createWaterContainerScene()
{
    //Container params
    const float containerWidth = 14.0f;
    const float containerHeight = 25.0f;
    const float containerDepth = 10.0f;
    const float spacing = particleRadius * 2.1f;

    const float centerX = (minBoundary.x + maxBoundary.x) * 0.5f;
    const float baseY = minBoundary.y + particleRadius * 2.0f;
    const float centerZ = (minBoundary.z + maxBoundary.z) * 0.5f;

    const float containerStartX = centerX - containerWidth * 0.5f;
    const float containerStartZ = centerZ - containerDepth * 0.5f;

    const int containerNumX = static_cast<int>(containerWidth / spacing);
    const int containerNumY = static_cast<int>(containerHeight / spacing);
    const int containerNumZ = static_cast<int>(containerDepth / spacing);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.001f, 0.001f);

    //function to add a particle
    auto addParticle = [&](glm::vec3 pos, glm::vec3 color, glm::vec3 velocity = glm::vec3(0.0f)) {
        Particle p;

        p.position = pos + glm::vec3(jitter(gen) * spacing * 0.01f,jitter(gen) * spacing * 0.01f,jitter(gen) * spacing * 0.01f);
        p.position.x = std::clamp(p.position.x, minBoundary.x + particleRadius * 1.5f, maxBoundary.x - particleRadius * 1.5f);
        p.position.y = std::clamp(p.position.y, minBoundary.y + particleRadius * 1.5f, maxBoundary.y - particleRadius * 1.5f);
        p.position.z = std::clamp(p.position.z, minBoundary.z + particleRadius * 1.5f, maxBoundary.z - particleRadius * 1.5f);

        p.padding1 = 0.0f;
        p.velocity = velocity;
        p.padding2 = 0.0f;
        p.predictedPosition = p.position;
        p.padding3 = 0.0f;
        p.color = color;
        p.padding4 = 0.0f;

        particles.push_back(p);
        };


    for (int x = 0; x < containerNumX; ++x) {
        for (int y = 0; y < containerNumY; ++y) {
            for (int z = 0; z < containerNumZ; ++z) {
                glm::vec3 pos(containerStartX + x * spacing, baseY + y * spacing, containerStartZ + z * spacing);

                float heightRatio = static_cast<float>(y) / containerNumY;
                glm::vec3 color(0.0f, 0.3f + 0.2f * heightRatio, 0.8f - 0.1f * heightRatio);
                addParticle(pos, color);
            }
        }
    }

    std::cout << "[PBFSystem] Created " << particles.size() << " particles for water container scene\n";
}

void PBFSystem::dropWaterBlock()
{
    //block params
    const float dropBlockWidth = 8.0f;
    const float dropBlockHeight = 16.0f;
    const float dropBlockDepth = 8.0f;
    const float dropHeight = 40.0f;


    const float spacing = particleRadius * 2.1f;
    const float centerX = (minBoundary.x + maxBoundary.x) * 0.5f;
    const float baseY = minBoundary.y + particleRadius * 2.0f;
    const float centerZ = (minBoundary.z + maxBoundary.z) * 0.5f;

    //highest existing water particle to place block above it
    float highestY = baseY;
    for (const auto& particle : particles) {
        highestY = std::max(highestY, particle.position.y);
    }

    const float dropBlockStartX = centerX - dropBlockWidth * 0.5f;
    const float dropBlockStartY = highestY + dropHeight;
    const float dropBlockStartZ = centerZ - dropBlockDepth * 0.5f;


    const int dropNumX = static_cast<int>(dropBlockWidth / spacing);
    const int dropNumY = static_cast<int>(dropBlockHeight / spacing);
    const int dropNumZ = static_cast<int>(dropBlockDepth / spacing);

    //random jitter for breaking symmetry
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.001f, 0.001f);

    //existing particles
    size_t existingParticles = particles.size();

    for (int x = 0; x < dropNumX; ++x) {
        for (int y = 0; y < dropNumY; ++y) {
            for (int z = 0; z < dropNumZ; ++z) {
                Particle p;
                p.position = glm::vec3(dropBlockStartX + x * spacing + jitter(gen) * spacing * 0.01f,dropBlockStartY + y * spacing + jitter(gen) * spacing * 0.01f,dropBlockStartZ + z * spacing + jitter(gen) * spacing * 0.01f);

                //boundary constraints
                p.position.x = std::clamp(p.position.x,minBoundary.x + particleRadius * 1.5f,maxBoundary.x - particleRadius * 1.5f);
                p.position.y = std::clamp(p.position.y,minBoundary.y + particleRadius * 1.5f,maxBoundary.y - particleRadius * 1.5f);
                p.position.z = std::clamp(p.position.z,minBoundary.z + particleRadius * 1.5f,maxBoundary.z - particleRadius * 1.5f);

                
                p.padding1 = 0.0f;
                p.velocity = glm::vec3(0.0f);
                p.padding2 = 0.0f;
                p.predictedPosition = p.position;
                p.padding3 = 0.0f;

                //gradient color
                float heightRatio = static_cast<float>(y) / dropNumY;
                p.color = glm::vec3(0.8f + 0.2f * heightRatio, 0.4f - 0.2f * heightRatio,0.0f);
                p.padding4 = 0.0f;

                particles.push_back(p);
            }
        }
    }

    std::cout << "[PBFSystem] Added " << (particles.size() - existingParticles)<< " particles for water block (total: " << particles.size() << ")\n";
}

void PBFSystem::initializeGPURendering() {
    // Create the shader program for GPU rendering
    try {
        // Debug info
        std::cout << "[PBFSystem] Loading shaders from: " << RESOURCES_PATH << "ssbo_render.vert and "
            << RESOURCES_PATH << "fragment.frag" << std::endl;

        // Check if vertex shader file exists
        std::ifstream vertFile(RESOURCES_PATH"ssbo_render.vert");
        if (!vertFile.is_open()) {
            std::cerr << "[PBFSystem] ERROR: Failed to open vertex shader file!" << std::endl;
            return;
        }

        // Debug: Print first few lines of shader code
        std::cout << "[PBFSystem] Shader file content preview:" << std::endl;
        std::string line;
        int lineCounter = 0;
        while (std::getline(vertFile, line) && lineCounter < 5) {
            std::cout << line << std::endl;
            lineCounter++;
        }
        vertFile.close();

        // Create a new shader program using our direct-access vertex shader
        Shader* shader = new Shader(RESOURCES_PATH"ssbo_render.vert", RESOURCES_PATH"fragment.frag");
        gpuShaderProgram = shader->ID;
        std::cout << "[PBFSystem] Shader compilation succeeded, program ID: " << gpuShaderProgram << std::endl;

        // Check if the program is valid
        GLint isLinked = 0;
        glGetProgramiv(gpuShaderProgram, GL_LINK_STATUS, &isLinked);
        if (isLinked != GL_TRUE) {
            GLint logLength = 0;
            glGetProgramiv(gpuShaderProgram, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> errorLog(logLength);
            glGetProgramInfoLog(gpuShaderProgram, logLength, NULL, errorLog.data());
            std::cerr << "[PBFSystem] Program linking error: " << errorLog.data() << std::endl;

            // Try to get individual shader compile logs
            GLuint vertShader = 0, fragShader = 0;
            GLint shaderCount = 0;
            glGetProgramiv(gpuShaderProgram, GL_ATTACHED_SHADERS, &shaderCount);
            if (shaderCount > 0) {
                std::vector<GLuint> shaders(shaderCount);
                glGetAttachedShaders(gpuShaderProgram, shaderCount, NULL, shaders.data());
                for (GLuint shaderId : shaders) {
                    GLint shaderType = 0;
                    glGetShaderiv(shaderId, GL_SHADER_TYPE, &shaderType);
                    GLint compileStatus = 0;
                    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);
                    GLint logLen = 0;
                    glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLen);
                    if (logLen > 0) {
                        std::vector<char> shaderLog(logLen);
                        glGetShaderInfoLog(shaderId, logLen, NULL, shaderLog.data());
                        std::cerr << "[PBFSystem] "
                            << (shaderType == GL_VERTEX_SHADER ? "Vertex" : "Fragment")
                            << " shader compilation "
                            << (compileStatus ? "succeeded" : "failed") << ": "
                            << shaderLog.data() << std::endl;
                    }
                }
            }

            // Delete the shader object as it's not valid
            delete shader;
            gpuShaderProgram = 0;
            return;
        }

        // We can delete the shader object after extracting its ID
        delete shader;
    }
    catch (const std::exception& e) {
        std::cerr << "[PBFSystem] Failed to create GPU rendering shader: " << e.what() << std::endl;
        return;
    }

    // Create VAO and VBO for rendering
    if (gpuRenderVAO == 0) glGenVertexArrays(1, &gpuRenderVAO);
    if (gpuRenderVBO == 0) glGenBuffers(1, &gpuRenderVBO);

    // Check if created successfully
    if (gpuRenderVAO == 0 || gpuRenderVBO == 0) {
        std::cerr << "[PBFSystem] Failed to create VAO or VBO" << std::endl;
        return;
    }

    glBindVertexArray(gpuRenderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gpuRenderVBO);

    // Generate an array of indices [0, 1, 2, ..., numParticles-1]
    std::vector<GLuint> indices(particles.size());
    for (GLuint i = 0; i < particles.size(); ++i) {
        indices[i] = i;
    }

    // Upload indices to VBO
    glBufferData(GL_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // Set up attribute pointer for the indices
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);

    // Check for any OpenGL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[PBFSystem] OpenGL error during initialization: 0x" << std::hex << err << std::dec << std::endl;
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    std::cout << "[PBFSystem] GPU rendering initialized with " << indices.size() << " particles" << std::endl;
}

void PBFSystem::renderParticlesGPU(Camera& camera, int screenWidth, int screenHeight) {
    if (!computeSystemInitialized) {
        std::cerr << "[PBFSystem] Cannot render from GPU: compute system not initialized" << std::endl;
        return;
    }

    // Get current particle count from compute system
    unsigned int particleCount = computeSystem->getNumParticles();

    // Debug output to verify counts match
    static unsigned int lastParticleCount = 0;
    if (particleCount != lastParticleCount) {
        std::cout << "[PBFSystem] Rendering " << particleCount << " particles." << std::endl;
        lastParticleCount = particleCount;
    }

    // Initialize GPU rendering resources if not done yet
    if (gpuRenderVAO == 0) {
        initializeGPURendering();

        // Check if initialization was successful
        if (gpuRenderVAO == 0 || gpuShaderProgram == 0) {
            std::cerr << "[PBFSystem] Failed to initialize GPU rendering resources" << std::endl;
            return;
        }
    }

    // Make sure we have the right number of indices
    if (lastRenderedParticleCount != particleCount) {
        glBindVertexArray(gpuRenderVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gpuRenderVBO);

        std::vector<GLuint> indices(particleCount);
        for (GLuint i = 0; i < particleCount; ++i) {
            indices[i] = i;
        }

        glBufferData(GL_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        lastRenderedParticleCount = particleCount;
        std::cout << "[PBFSystem] Updated rendering buffer with " << particleCount << " indices" << std::endl;
    }

    // Check for any OpenGL errors before proceeding
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[PBFSystem] OpenGL error before shader use: 0x" << std::hex << err << std::dec << std::endl;
    }

    // Ensure the particle SSBO is bound to binding point 1 BEFORE using the shader
    GLuint particleBufferId = computeSystem->getParticleBufferId();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleBufferId);

    // Check for any OpenGL errors after binding SSBO
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[PBFSystem] OpenGL error after binding SSBO: 0x" << std::hex << err << std::dec << std::endl;
    }

    // Use the direct rendering shader
    glUseProgram(gpuShaderProgram);

    // Check if program is valid
    GLint isProgram = 0;
    glGetProgramiv(gpuShaderProgram, GL_LINK_STATUS, &isProgram);
    if (isProgram != GL_TRUE) {
        std::cerr << "[PBFSystem] Shader program is not linked successfully" << std::endl;

        // Get error info
        GLint logLength = 0;
        glGetProgramiv(gpuShaderProgram, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            std::vector<char> errorLog(logLength);
            glGetProgramInfoLog(gpuShaderProgram, logLength, NULL, errorLog.data());
            std::cerr << "Program linking error: " << errorLog.data() << std::endl;
        }

        // Abort rendering
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        return;
    }

    // Check for any OpenGL errors after using shader
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[PBFSystem] OpenGL error after shader use: 0x" << std::hex << err << std::dec << std::endl;
    }

    // Get uniform locations
    GLint modelLoc = glGetUniformLocation(gpuShaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(gpuShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(gpuShaderProgram, "projection");
    GLint particleRadiusLoc = glGetUniformLocation(gpuShaderProgram, "particleRadius");
    GLint viewPosLoc = glGetUniformLocation(gpuShaderProgram, "viewPos");
    GLint lightPosLoc = glGetUniformLocation(gpuShaderProgram, "lightPos");

    // Set uniforms using the provided camera and screen dimensions
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
        (float)screenWidth / (float)screenHeight,
        0.1f, 1000.0f);

    if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    if (particleRadiusLoc != -1) glUniform1f(particleRadiusLoc, particleRadius);
    if (viewPosLoc != -1) glUniform3f(viewPosLoc, camera.Position.x, camera.Position.y, camera.Position.z);
    if (lightPosLoc != -1) glUniform3f(lightPosLoc, 10.0f, 10.0f, 10.0f);

    // Check for any OpenGL errors after setting uniforms
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[PBFSystem] OpenGL error after setting uniforms: 0x" << std::hex << err << std::dec << std::endl;
    }

    // Draw the particles
    glBindVertexArray(gpuRenderVAO);

    // Check for any OpenGL errors after binding VAO
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[PBFSystem] OpenGL error after binding VAO: 0x" << std::hex << err << std::dec << std::endl;
    }

    // Draw the particles
    glDrawArrays(GL_POINTS, 0, particleCount);

    // Check for any OpenGL errors after drawing
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[PBFSystem] OpenGL error after drawing: 0x" << std::hex << err << std::dec << std::endl;
    }

    glBindVertexArray(0);

    // Unbind SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
}