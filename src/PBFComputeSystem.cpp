#include "PBFComputeSystem.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>

PBFComputeSystem::PBFComputeSystem(): externalForcesShader(nullptr), constructGridShader(nullptr), clearGridShader(nullptr), densityShader(nullptr), positionUpdateShader(nullptr), vorticityViscosityShader(nullptr), velocityUpdateShader(nullptr), simParamsUBO(0),particleSSBO(0),numParticles(0),maxParticles(0)
{
}

PBFComputeSystem::~PBFComputeSystem() {
    cleanup();
}

bool PBFComputeSystem::initialize(unsigned int maxParticles, float dt, const glm::vec4& gravity, float particleRadius, float smoothingLength, const glm::vec4& minBoundary, const glm::vec4& maxBoundary, float cellSize, unsigned int maxParticlesPerCell,float restDensity, float vorticityEpsilon, float xsphViscosityCoeff) {
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

        densityShader = new ComputeShader(RESOURCES_PATH"calculate_density.comp");
        std::cout << "[PBFComputeSystem] Density shader loaded successfully (ID=" << densityShader->ID << ")\n";

        positionUpdateShader = new ComputeShader(RESOURCES_PATH"apply_position_update.comp");
        std::cout << "[PBFComputeSystem] Position update shader loaded successfully (ID=" << positionUpdateShader->ID << ")\n";

        vorticityViscosityShader = new ComputeShader(RESOURCES_PATH"apply_vorticity_viscosity.comp");
        std::cout << "[PBFComputeSystem] Vorticity and viscosity shader loaded successfully (ID=" << vorticityViscosityShader->ID << ")\n";

        velocityUpdateShader = new ComputeShader(RESOURCES_PATH"update_velocity.comp");
        std::cout << "[PBFComputeSystem] Velocity update shader loaded successfully (ID=" << velocityUpdateShader->ID << ")\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[PBFComputeSystem] Failed to load compute shader: "<< e.what() << std::endl;
        return false;
    }

    // Create GPU buffers
    createBuffers(maxParticles);

	// Initialize simulation parameters
	params.dt = dt;
	params.gravity = gravity;
	params.particleRadius = particleRadius;
	params.h = smoothingLength;
    params.minBoundary = minBoundary;
    params.maxBoundary = maxBoundary;
	params.cellSize = cellSize;
	params.maxParticlesPerCell = maxParticlesPerCell;
    params.restDensity = restDensity;
    params.vorticityEpsilon = vorticityEpsilon;
    params.xsphViscosityCoeff = xsphViscosityCoeff;

    initializeGrid();

    return true;
}

void PBFComputeSystem::createBuffers(unsigned int maxParticles) {
    //Simulation parameters uniform buffer - CPU write, GPU read
    glGenBuffers(1, &simParamsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SimParams), &params, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    std::cout << "[PBFComputeSystem] Created simulation params UBO (ID="<< simParamsUBO << ")\n";

    //Particle buffer
    glGenBuffers(1, &particleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxParticles * sizeof(Particle), nullptr, GL_DYNAMIC_READ);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    std::cout << "[PBFComputeSystem] Created particle SSBO (ID="<< particleSSBO << ")\n";
}


void PBFComputeSystem::updateSimulationParams(float dt,const glm::vec4& gravity,float particleRadius,float smoothingLength,const glm::vec4& minBoundary,const glm::vec4& maxBoundary, float cellSize, unsigned int maxParticlesPerCell,float restDensity, float vorticityEpsilon, float xsphViscosityCoeff) {
    params.dt = dt;
    params.gravity = gravity;
	params.particleRadius = particleRadius;
	params.h = smoothingLength;
	params.minBoundary = minBoundary;
	params.maxBoundary = maxBoundary;
	params.cellSize = cellSize;
	params.maxParticlesPerCell = maxParticlesPerCell;
	params.restDensity = restDensity;
	params.vorticityEpsilon = vorticityEpsilon;
	params.xsphViscosityCoeff = xsphViscosityCoeff;

    
    //Upload to GPU
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
        numParticles = maxParticles;
    }

    glFinish();

    try {
        if (particles.size() != numParticles)
            particles.resize(numParticles);
    }
    catch (const std::exception& e) {
        std::cerr << "[PBFComputeSystem] ERROR: Failed to resize particles vector: " << e.what() << std::endl;
        return;
    }

    //Download particles data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    GLint bufferSize = 0;
    glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &bufferSize);

    if (bufferSize < (GLint)(numParticles * sizeof(Particle))) {
        std::cerr << "[PBFComputeSystem] ERROR: Buffer size too small for " << numParticles << " particles!\n";
        return;
    }

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, numParticles * sizeof(Particle), particles.data());
  
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void PBFComputeSystem::step() {
    if (numParticles == 0) {
        std::cerr << "[PBFComputeSystem] Warning: step called with zero particles\n";
        return;
    }

    applyExternalForces();

    findNeighbors();

    const int solverIterations = 3;
    for (int iter = 0; iter < solverIterations; iter++) {
        calculateDensity();
        applyPositionUpdate();
    }
    
    updateVelocity();
    applyVorticityViscosity();
}

void PBFComputeSystem::applyExternalForces() {
    //work groups
    unsigned int numGroups = (numParticles + 255) / 256;
    if (numGroups == 0) numGroups = 1;

    //Update simParamsUBO
    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParams), &params);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Activate the external forces compute shader
    glUseProgram(externalForcesShader->ID);

    //Bind buffers
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleSSBO);

    //Dispatch
    glDispatchCompute(numGroups, 1, 1);

    //ensure compute shader has completed
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void PBFComputeSystem::initializeGrid() {

    glm::vec3 domain = params.maxBoundary - params.minBoundary;
    glm::ivec3 gridDim = glm::ivec3(glm::ceil(domain / params.cellSize));

    int totalCells = gridDim.x * gridDim.y * gridDim.z;

    std::cout << "[PBFComputeSystem] Grid dimensions: " << gridDim.x << "x"<< gridDim.y << "x" << gridDim.z << " (" << totalCells << " cells)\n";

    glGenBuffers(1, &cellCountsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellCountsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, totalCells * sizeof(GLuint), nullptr, GL_DYNAMIC_READ);

    glGenBuffers(1, &cellParticlesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellParticlesBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,totalCells * params.maxParticlesPerCell * sizeof(GLuint),nullptr, GL_DYNAMIC_COPY);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void PBFComputeSystem::findNeighbors() {
    glm::vec3 domain = params.maxBoundary - params.minBoundary;
    glm::ivec3 gridDim = glm::ivec3(glm::ceil(domain / params.cellSize));
    int totalCells = gridDim.x * gridDim.y * gridDim.z;

    unsigned int clearGroups = (totalCells + 255) / 256;
    if (clearGroups == 0) clearGroups = 1;

    unsigned int particleGroups = (numParticles + 255) / 256;
    if (particleGroups == 0) particleGroups = 1;

    params.numParticles = numParticles;
    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParams), &params);

    clearGridShader->use();

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsBuffer);

    glDispatchCompute(clearGroups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    constructGridShader->use();

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, cellParticlesBuffer);

    glDispatchCompute(particleGroups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void PBFComputeSystem::calculateDensity() {
    if (numParticles == 0) {
        std::cerr << "[PBFComputeSystem] Warning: calculateDensity called with zero particles\n";
        return;
    }

    unsigned int numGroups = (numParticles + 255) / 256;
    if (numGroups == 0) numGroups = 1;

    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParams), &params);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    densityShader->use();

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, cellParticlesBuffer);

    glDispatchCompute(numGroups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void PBFComputeSystem::applyPositionUpdate() {
    unsigned int numGroups = (numParticles + 255) / 256;
    if (numGroups == 0) numGroups = 1;

    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParams), &params);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    positionUpdateShader->use();

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, cellParticlesBuffer);

    glDispatchCompute(numGroups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void PBFComputeSystem::applyVorticityViscosity() {
    unsigned int numGroups = (numParticles + 255) / 256;
    if (numGroups == 0) numGroups = 1;

    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParams), &params);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    vorticityViscosityShader->use();

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, cellParticlesBuffer);

    glDispatchCompute(numGroups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void PBFComputeSystem::updateVelocity() {
    unsigned int numGroups = (numParticles + 255) / 256;
    if (numGroups == 0) numGroups = 1;

    glBindBuffer(GL_UNIFORM_BUFFER, simParamsUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SimParams), &params);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    velocityUpdateShader->use();

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, simParamsUBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleSSBO);

    glDispatchCompute(numGroups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

bool PBFComputeSystem::checkComputeShaderSupport() {
    GLint maxComputeWorkGroupCount[3] = { 0 };
    GLint maxComputeWorkGroupSize[3] = { 0 };
    GLint maxComputeWorkGroupInvocations = 0;

    GLint numShaderTypes = 0;
    glGetIntegerv(GL_NUM_SHADING_LANGUAGE_VERSIONS, &numShaderTypes);

    std::cout << "=== OpenGL Compute Shader Capability Check ===\n";
    std::cout << "GL_VENDOR:   " << glGetString(GL_VENDOR) << "\n";
    std::cout << "GL_RENDERER: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "GL_VERSION:  " << glGetString(GL_VERSION) << "\n";

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxComputeWorkGroupCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxComputeWorkGroupCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &maxComputeWorkGroupCount[2]);

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxComputeWorkGroupSize[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxComputeWorkGroupSize[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxComputeWorkGroupSize[2]);

    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxComputeWorkGroupInvocations);

    std::cout << "Max compute work group count: "<< maxComputeWorkGroupCount[0] << ", "<< maxComputeWorkGroupCount[1] << ", "<< maxComputeWorkGroupCount[2] << "\n";
    std::cout << "Max compute work group size:  "<< maxComputeWorkGroupSize[0] << ", "<< maxComputeWorkGroupSize[1] << ", "<< maxComputeWorkGroupSize[2] << "\n";
    std::cout << "Max compute work group invocations: "<< maxComputeWorkGroupInvocations << "\n";


    GLint maxComputeUniformBlocks = 0;
    glGetIntegerv(GL_MAX_COMPUTE_UNIFORM_BLOCKS, &maxComputeUniformBlocks);
    std::cout << "Max compute uniform blocks: "<< maxComputeUniformBlocks << "\n";

    GLint maxComputeUniformComponents = 0;
    glGetIntegerv(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, &maxComputeUniformComponents);
    std::cout << "Max compute uniform components: "
        << maxComputeUniformComponents << "\n";

    GLint maxUniformBlockSize = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    std::cout << "Max uniform block size (bytes): "<< maxUniformBlockSize << "\n";

    GLint maxUniformBufferBindings = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUniformBufferBindings);
    std::cout << "Max uniform buffer bindings: "<< maxUniformBufferBindings << "\n";

    GLint maxShaderStorageBufferBindings = 0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxShaderStorageBufferBindings);
    std::cout << "Max shader storage buffer bindings: "<< maxShaderStorageBufferBindings << "\n";

    GLint maxShaderStorageBlockSize = 0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxShaderStorageBlockSize);
    std::cout << "Max shader storage block size (bytes): "<< maxShaderStorageBlockSize << "\n";

    bool supported = (maxComputeWorkGroupSize[0] > 0);
    std::cout << "Compute shaders supported: " << (supported ? "YES" : "NO") << "\n";

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error during capability check: 0x"<< std::hex << err << std::dec << "\n";
        supported = false;
    }

    std::cout << "=== End Capability Check ===\n";
    return supported;
}

void PBFComputeSystem::recordDensityStatistics(const std::string& filename) {
    if (numParticles == 0) {
        std::cerr << "[PBFComputeSystem] Warning: recordDensityStatistics called with zero particles\n";
        return;
    }

    //Download particle density data
    std::vector<Particle> particles(numParticles);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, numParticles * sizeof(Particle), particles.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    //Calculate average and maximum density
    float totalDensity = 0.0f;
    float maxDensity = 0.0f;

    for (const auto& particle : particles) {
        totalDensity += particle.density;
        maxDensity = std::max(maxDensity, particle.density);
    }

    float avgDensity = totalDensity / numParticles;

    // Create or append to CSV file
    static bool fileExists = false;
    std::ofstream file;

    if (!fileExists) {
        // Create new file with headers
        file.open(filename);
        if (file.is_open()) {
            file << "Frame,AverageDensity,MaximumDensity,RestDensity\n";
            fileExists = true;
        }
    }
    else {
        // Append to existing file
        file.open(filename, std::ios::app);
    }

    if (file.is_open()) {
        static int frameCount = 0;
        file << ++frameCount << ","
            << avgDensity << ","
            << maxDensity << ","
            << params.restDensity << "\n";
        file.close();
    }
    else {
        std::cerr << "[PBFComputeSystem] Failed to open density log file: " << filename << std::endl;
    }
}

void PBFComputeSystem::cleanup() {
    // Delete compute shaders
    delete externalForcesShader;
    externalForcesShader = nullptr;

    delete constructGridShader;
    constructGridShader = nullptr;

    delete clearGridShader;
    clearGridShader = nullptr;

    delete densityShader;
    densityShader = nullptr;

    delete positionUpdateShader;
    positionUpdateShader = nullptr;

    delete vorticityViscosityShader;
    vorticityViscosityShader = nullptr;
    
    delete velocityUpdateShader;
    velocityUpdateShader = nullptr;

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

