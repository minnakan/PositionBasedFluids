#include "PBFComputeSystem.h"
#include <iostream>
#include <iomanip>
#include <sstream> // for better logs

PBFComputeSystem::PBFComputeSystem(): externalForcesShader(nullptr), constructGridShader(nullptr), clearGridShader(nullptr),simParamsUBO(0),particleSSBO(0),numParticles(0),maxParticles(0)
{
}

PBFComputeSystem::~PBFComputeSystem() {
    cleanup();
}

bool PBFComputeSystem::initialize(unsigned int maxParticles, float dt, const glm::vec4& gravity, float particleRadius, float smoothingLength, const glm::vec4& minBoundary, const glm::vec4& maxBoundary, float cellSize, unsigned int maxParticlesPerCell) {
    // Store the maximum number of particles
    this->maxParticles = maxParticles;

	//checkComputeShaderSupport();

    // Create externalForces compute shader
    try {
        externalForcesShader = new ComputeShader(RESOURCES_PATH"external_forces.comp");
        std::cout << "[PBFComputeSystem] Compute shader loaded successfully (ID="<< externalForcesShader->ID << ")\n";

        constructGridShader = new ComputeShader(RESOURCES_PATH"construct_grid.comp");
        std::cout << "[PBFComputeSystem] Construct grid shader loaded successfully (ID="<< constructGridShader->ID << ")\n";

        clearGridShader = new ComputeShader(RESOURCES_PATH"clear_grid.comp");
        std::cout << "[PBFComputeSystem] Clear grid shader loaded successfully (ID=" << constructGridShader->ID << ")\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[PBFComputeSystem] Failed to load compute shader: "
            << e.what() << std::endl;
        return false;
    }

    // Create GPU buffers
    createBuffers(maxParticles);

	// Initialize simulation parameters
	params.dt = dt;
	params.gravity = gravity;
	params.particleRadius = particleRadius;
	params.h = smoothingLength;
    params.cellSize = 0.2f;
    params.maxParticlesPerCell = 64;
    params.minBoundary = minBoundary;
    params.maxBoundary = maxBoundary;
	params.cellSize = cellSize;
	params.maxParticlesPerCell = maxParticlesPerCell;

    initializeGrid();

    return true;
}

void PBFComputeSystem::createBuffers(unsigned int maxParticles) {
    // Simulation parameters uniform buffer - CPU write, GPU read
    glGenBuffers(1, &simParamsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SimParams), &params, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    std::cout << "[PBFComputeSystem] Created simulation params UBO (ID="<< simParamsUBO << ")\n";

    // Particle buffer
    glGenBuffers(1, &particleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxParticles * sizeof(Particle), nullptr, GL_DYNAMIC_READ);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    std::cout << "[PBFComputeSystem] Created particle SSBO (ID="<< particleSSBO << ")\n";
}


void PBFComputeSystem::updateSimulationParams(float dt,const glm::vec4& gravity,float particleRadius,float smoothingLength,const glm::vec4& minBoundary,const glm::vec4& maxBoundary, float cellSize, unsigned int maxParticlesPerCell) {
    // Update local params struct
    params.dt = dt;
    params.gravity = gravity;
	params.particleRadius = particleRadius;
	params.h = smoothingLength;
	params.minBoundary = minBoundary;
	params.maxBoundary = maxBoundary;
	params.cellSize = cellSize;
	params.maxParticlesPerCell = maxParticlesPerCell;

    
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
        std::cerr << "[PBFComputeSystem] Warning: Attempting to upload "<< particles.size() << " but max is " << maxParticles << "\n";
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

    if (numParticles > maxParticles) {
        std::cerr << "[PBFComputeSystem] ERROR: numParticles (" << numParticles << ") exceeds maxParticles (" << maxParticles << ")\n";
        numParticles = maxParticles; // Cap it to avoid overflow
    }

    // Ensure compute finishes
    glFinish();

    try {
        //std::cout << "[PBFComputeSystem] Resizing particle array to " << numParticles << " particles\n";
        if (particles.size() != numParticles)
            particles.resize(numParticles);
    }
    catch (const std::exception& e) {
        std::cerr << "[PBFComputeSystem] ERROR: Failed to resize particles vector: " << e.what() << std::endl;
        return;
    }

    // Download particles data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    GLint bufferSize = 0;
    glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &bufferSize);

    if (bufferSize < (GLint)(numParticles * sizeof(Particle))) {
        std::cerr << "[PBFComputeSystem] ERROR: Buffer size too small for " << numParticles << " particles!\n";
        return;
    }

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, numParticles * sizeof(Particle), particles.data());

    // Download and log cell counts data
    static int frameCount = 0;
    frameCount++;

    // Only log every 60 frames to avoid console spam
    if (frameCount % 60 == 0) {
        // Calculate grid dimensions
        glm::vec3 domain = params.maxBoundary - params.minBoundary;
        glm::ivec3 gridDim = glm::ivec3(glm::ceil(domain / params.cellSize));
        int totalCells = gridDim.x * gridDim.y * gridDim.z;

        // Download cell counts
        std::vector<GLuint> cellCounts(totalCells);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellCountsBuffer);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalCells * sizeof(GLuint), cellCounts.data());

        // Analyze cell statistics
        GLuint maxCount = 0;
        GLuint totalFilledCells = 0;
        GLuint totalParticlesInCells = 0;
        GLuint cellsExceedingLimit = 0;

        for (int i = 0; i < totalCells; i++) {
            if (cellCounts[i] > 0) {
                totalFilledCells++;
                totalParticlesInCells += cellCounts[i];
                maxCount = std::max(maxCount, cellCounts[i]);

                if (cellCounts[i] > params.maxParticlesPerCell) {
                    cellsExceedingLimit++;
                }
            }
        }

        float avgParticlesPerFilledCell = totalFilledCells > 0 ?
            static_cast<float>(totalParticlesInCells) / totalFilledCells : 0.0f;

        std::cout << "===== Cell Statistics (Frame " << frameCount << ") =====\n";
        std::cout << "Total cells: " << totalCells << "\n";
        std::cout << "Filled cells: " << totalFilledCells << " ("
            << (totalFilledCells * 100.0f / totalCells) << "%)\n";
        std::cout << "Max particles in a cell: " << maxCount << "\n";
        std::cout << "Avg particles per filled cell: " << avgParticlesPerFilledCell << "\n";

        if (cellsExceedingLimit > 0) {
            std::cout << "WARNING: " << cellsExceedingLimit << " cells exceed max particles limit of "
                << params.maxParticlesPerCell << "!\n";
        }

        // Find and print the 5 most populated cells without sorting
        std::cout << "Top 5 most populated cells:\n";

        // Just do a simple search for the top 5 cells
        for (int topN = 0; topN < 5; topN++) {
            int maxIndex = -1;
            GLuint maxValue = 0;

            // Find the next highest value
            for (int i = 0; i < totalCells; i++) {
                if (cellCounts[i] > maxValue) {
                    bool alreadyFound = false;

                    // Check if we've already printed this cell
                    for (int j = 0; j < topN; j++) {
                        // We would need to store previous maxIndices
                        // This is a simplified version that just finds the highest
                        // and prints it multiple times for illustration
                    }

                    if (!alreadyFound) {
                        maxIndex = i;
                        maxValue = cellCounts[i];
                    }
                }
            }

            if (maxIndex >= 0) {
                // Convert 1D index back to 3D coordinates
                int z = maxIndex / (gridDim.x * gridDim.y);
                int remainder = maxIndex - z * gridDim.x * gridDim.y;
                int y = remainder / gridDim.x;
                int x = remainder % gridDim.x;

                std::cout << "  Cell [" << x << "," << y << "," << z << "] (index "
                    << maxIndex << "): " << maxValue << " particles\n";

                // Zero out this max so we find the next one
                cellCounts[maxIndex] = 0;
            }
        }

        std::cout << "=======================================\n";
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void PBFComputeSystem::step() {
    if (numParticles == 0) {
        std::cerr << "[PBFComputeSystem] Warning: step called with zero particles\n";
        return;
    }

    

    applyExternalForces();

    findNeighbors();
}

void PBFComputeSystem::applyExternalForces() {
    // Calculate # of work groups
    unsigned int numGroups = (numParticles + 255) / 256;
    if (numGroups == 0) numGroups = 1;

    // Update simParamsUBO
    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParams), &params);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Activate the external forces compute shader
    glUseProgram(externalForcesShader->ID);

    // Bind buffers
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleSSBO);

    // Dispatch compute shader
    glDispatchCompute(numGroups, 1, 1);

    // Memory barrier to ensure compute shader has completed
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void PBFComputeSystem::initializeGrid() {
    // Calculate grid dimensions
    glm::vec3 domain = params.maxBoundary - params.minBoundary;
    glm::ivec3 gridDim = glm::ivec3(glm::ceil(domain / params.cellSize));

    // Calculate total cells
    int totalCells = gridDim.x * gridDim.y * gridDim.z;

    std::cout << "[PBFComputeSystem] Grid dimensions: " << gridDim.x << "x"<< gridDim.y << "x" << gridDim.z << " (" << totalCells << " cells)\n";

    // Create buffer for cell counts
    glGenBuffers(1, &cellCountsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellCountsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, totalCells * sizeof(GLuint), nullptr, GL_DYNAMIC_READ);

    // Create buffer for cell particles
    glGenBuffers(1, &cellParticlesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellParticlesBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,totalCells * params.maxParticlesPerCell * sizeof(GLuint),nullptr, GL_DYNAMIC_COPY);

    // Unbind
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void PBFComputeSystem::findNeighbors() {
    // Calculate grid dimensions
    glm::vec3 domain = params.maxBoundary - params.minBoundary;
    glm::ivec3 gridDim = glm::ivec3(glm::ceil(domain / params.cellSize));
    int totalCells = gridDim.x * gridDim.y * gridDim.z;

    // Calculate work groups for cell clearing
    unsigned int clearGroups = (totalCells + 255) / 256;
    if (clearGroups == 0) clearGroups = 1;

    // Calculate work groups for particle processing
    unsigned int particleGroups = (numParticles + 255) / 256;
    if (particleGroups == 0) particleGroups = 1;

    // Make sure numParticles is up to date in the simulation parameters
    params.numParticles = numParticles;
    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParams), &params);

    // STEP 1: Clear the grid cell counts
    clearGridShader->use();

    // Bind the necessary buffers
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsBuffer);

    // Dispatch the clear grid shader
    glDispatchCompute(clearGroups, 1, 1);

    // Ensure clearing has completed before populating
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // STEP 2: Populate the grid with particles
    constructGridShader->use();

    // Bind the necessary buffers
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, cellParticlesBuffer);

    // Dispatch the construct grid shader
    glDispatchCompute(particleGroups, 1, 1);

    // Final memory barrier to ensure all writes have completed
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
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

void PBFComputeSystem::cleanup() {
    // Delete compute shaders
    delete externalForcesShader;
    externalForcesShader = nullptr;

    delete constructGridShader;
    constructGridShader = nullptr;

    delete clearGridShader;
    clearGridShader = nullptr;

    // Delete existing GPU buffers
    if (simParamsUBO) glDeleteBuffers(1, &simParamsUBO);
    if (particleSSBO) glDeleteBuffers(1, &particleSSBO);

    // Delete grid buffers
    if (cellCountsBuffer) glDeleteBuffers(1, &cellCountsBuffer);
    if (cellParticlesBuffer) glDeleteBuffers(1, &cellParticlesBuffer);

    // Reset buffer IDs
    simParamsUBO = 0;
    particleSSBO = 0;
    cellCountsBuffer = 0;
    cellParticlesBuffer = 0;
}

