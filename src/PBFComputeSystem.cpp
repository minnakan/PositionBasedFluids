#include "PBFComputeSystem.h"
#include <iostream>
#include <iomanip>
#include <sstream> // for better logs

PBFComputeSystem::PBFComputeSystem(): computeShader(nullptr),simParamsUBO(0),particleSSBO(0),numParticles(0),maxParticles(0)
{
}

PBFComputeSystem::~PBFComputeSystem() {
    cleanup();
}

bool PBFComputeSystem::initialize(unsigned int maxParticles) {
    // Store the maximum number of particles
    this->maxParticles = maxParticles;

	//checkComputeShaderSupport();

    // Create compute shader
    try {
        computeShader = new ComputeShader(RESOURCES_PATH"external_forces.comp");
        std::cout << "[PBFComputeSystem] Compute shader loaded successfully (ID="
            << computeShader->ID << ")\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[PBFComputeSystem] Failed to load compute shader: "
            << e.what() << std::endl;
        return false;
    }

    // Create GPU buffers
    createBuffers(maxParticles);
    return true;
}

void PBFComputeSystem::createBuffers(unsigned int maxParticles) {
    // Simulation parameters uniform buffer - CPU write, GPU read
    glGenBuffers(1, &simParamsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SimParams), &params, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    std::cout << "[PBFComputeSystem] Created simulation params UBO (ID="
        << simParamsUBO << ")\n";

    // Particle buffer
    glGenBuffers(1, &particleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxParticles * sizeof(Particle), nullptr, GL_DYNAMIC_READ);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    std::cout << "[PBFComputeSystem] Created particle SSBO (ID="
        << particleSSBO << ")\n";
}

void PBFComputeSystem::cleanup() {
    // Delete compute shader
    delete computeShader;
    computeShader = nullptr;

    // Delete GPU buffers
    if (simParamsUBO) glDeleteBuffers(1, &simParamsUBO);
    if (particleSSBO) glDeleteBuffers(1, &particleSSBO);

    // Reset buffer IDs
    simParamsUBO = 0;
    particleSSBO = 0;
}

void PBFComputeSystem::updateSimulationParams(float dt,const glm::vec4& gravity,float particleRadius,float smoothingLength,const glm::vec4& minBoundary,const glm::vec4& maxBoundary) {
    // Update local params struct
    params.dt = dt;
    params.gravity = gravity;
	params.particleRadius = particleRadius;
	params.h = smoothingLength;
	params.minBoundary = minBoundary;
	params.maxBoundary = maxBoundary;

    
    // Upload to GPU
    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParams), &params);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void PBFComputeSystem::uploadParticles(const std::vector<Particle>& particles) {
    if (particles.empty()) {
        std::cerr << "[PBFComputeSystem] Warning: Trying to upload empty particle array\n";
        return;
    }

    if (particles.size() > maxParticles) {
        std::cerr << "[PBFComputeSystem] Warning: Attempting to upload "
            << particles.size() << " but max is " << maxParticles << "\n";
        numParticles = maxParticles;
    }
    else {
        numParticles = (unsigned int)particles.size();
    }

    // Upload
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, numParticles * sizeof(Particle), particles.data());
}

void PBFComputeSystem::downloadParticles(std::vector<Particle>& particles) {
    if (numParticles == 0) {
        std::cerr << "[PBFComputeSystem] Warning: No particles to download\n";
        return;
    }

    // Ensure compute finishes
    glFinish();

    particles.resize(numParticles);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    GLint bufferSize = 0;
    glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &bufferSize);

    if (bufferSize < (GLint)(numParticles * sizeof(Particle))) {
        std::cerr << "[PBFComputeSystem] ERROR: Buffer size too small for "
            << numParticles << " particles!\n";
        return;
    }

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, numParticles * sizeof(Particle), particles.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    static int frameCount = 0;
    frameCount++;
}

void PBFComputeSystem::step() {
    if (numParticles == 0) {
        std::cerr << "[PBFComputeSystem] Warning: step called with zero particles\n";
        return;
    }
    // Calculate # of work groups
    unsigned int numGroups = (numParticles + 255) / 256;
    if (numGroups == 0) numGroups = 1;

    static int frameCount = 0;
    frameCount++;

    //// Print frame info occasionally
    //if (frameCount % 60 == 0 || frameCount < 5) {
    //    std::cout << "[PBFComputeSystem] Frame " << frameCount << " - Dispatching compute shader...\n";
    //}

    // Read first particle before compute for verification
    Particle firstParticleBefore;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(Particle), &firstParticleBefore);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Update simParamsUBO
    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParams), &params);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Check for any OpenGL errors before activating the shader
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[PBFComputeSystem] OpenGL error before activating shader: 0x"
            << std::hex << err << std::dec << std::endl;
    }

    // Activate the compute shader directly with glUseProgram
    glUseProgram(computeShader->ID);

    // Check for errors after activation
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[PBFComputeSystem] OpenGL error after activating shader: 0x"
            << std::hex << err << std::dec << std::endl;
    }

    // Verify shader activation
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

    /*if (frameCount % 60 == 0 || frameCount < 5) {
        std::cout << "[PBFComputeSystem] Current GL program: " << currentProgram
            << " (Compute shader ID: " << computeShader->ID << ")\n";

        if (currentProgram != computeShader->ID) {
            std::cerr << "[PBFComputeSystem] ERROR: Compute shader not activated correctly!\n";
        }
    }*/

    // Bind buffers AFTER activating the shader
    // First unbind all, then rebind to avoid any state issues
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);

    // Then bind to the correct locations
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleSSBO);

    // Verify buffer bindings
    GLint boundUBO = 0, boundSSBO = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &boundUBO);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 1, &boundSSBO);

    // Dispatch compute shader
    glDispatchCompute(numGroups, 1, 1);

    // Check for errors after dispatch
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[PBFComputeSystem] OpenGL error after dispatch: 0x"
            << std::hex << err << std::dec << std::endl;
    }

    // Use a more comprehensive memory barrier
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // Synchronize to ensure compute shader has completed
    glFinish();
}

bool PBFComputeSystem::checkComputeShaderSupport() {
    GLint maxComputeWorkGroupCount[3] = { 0 };
    GLint maxComputeWorkGroupSize[3] = { 0 };
    GLint maxComputeWorkGroupInvocations = 0;

    // Check if compute shaders are supported
    GLint numShaderTypes = 0;
    glGetIntegerv(GL_NUM_SHADING_LANGUAGE_VERSIONS, &numShaderTypes);

    std::cout << "=== OpenGL Compute Shader Capability Check ===\n";
    std::cout << "GL_VENDOR:   " << glGetString(GL_VENDOR) << "\n";
    std::cout << "GL_RENDERER: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "GL_VERSION:  " << glGetString(GL_VERSION) << "\n";

    // Get max work group counts
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxComputeWorkGroupCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxComputeWorkGroupCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &maxComputeWorkGroupCount[2]);

    // Get max work group sizes
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxComputeWorkGroupSize[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxComputeWorkGroupSize[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxComputeWorkGroupSize[2]);

    // Get max work group invocations
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxComputeWorkGroupInvocations);

    std::cout << "Max compute work group count: "
        << maxComputeWorkGroupCount[0] << ", "
        << maxComputeWorkGroupCount[1] << ", "
        << maxComputeWorkGroupCount[2] << "\n";

    std::cout << "Max compute work group size:  "
        << maxComputeWorkGroupSize[0] << ", "
        << maxComputeWorkGroupSize[1] << ", "
        << maxComputeWorkGroupSize[2] << "\n";

    std::cout << "Max compute work group invocations: "
        << maxComputeWorkGroupInvocations << "\n";

    // -----------------------------------------------------------------------
    // Add uniform-related queries here:
    // -----------------------------------------------------------------------
    GLint maxComputeUniformBlocks = 0;
    glGetIntegerv(GL_MAX_COMPUTE_UNIFORM_BLOCKS, &maxComputeUniformBlocks);
    std::cout << "Max compute uniform blocks: "
        << maxComputeUniformBlocks << "\n";

    GLint maxComputeUniformComponents = 0;
    glGetIntegerv(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, &maxComputeUniformComponents);
    std::cout << "Max compute uniform components: "
        << maxComputeUniformComponents << "\n";

    GLint maxUniformBlockSize = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::cout << "Max uniform block size (bytes): "
        << maxUniformBlockSize << "\n";

    // You might also want to see how many UBO binding points you have in total:
    GLint maxUniformBufferBindings = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUniformBufferBindings);
    std::cout << "Max uniform buffer bindings: "
        << maxUniformBufferBindings << "\n";

    // If you need maximum SSBO bindings or maximum SSBO size:
    GLint maxShaderStorageBufferBindings = 0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxShaderStorageBufferBindings);
    std::cout << "Max shader storage buffer bindings: "
        << maxShaderStorageBufferBindings << "\n";

    GLint maxShaderStorageBlockSize = 0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxShaderStorageBlockSize);
    std::cout << "Max shader storage block size (bytes): "
        << maxShaderStorageBlockSize << "\n";

    // -----------------------------------------------------------------------
    // Determine overall compute support
    // -----------------------------------------------------------------------
    bool supported = (maxComputeWorkGroupSize[0] > 0);
    std::cout << "Compute shaders supported: " << (supported ? "YES" : "NO") << "\n";

    // Check for any OpenGL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error during capability check: 0x"
            << std::hex << err << std::dec << "\n";
        supported = false;
    }

    std::cout << "=== End Capability Check ===\n";
    return supported;
}

