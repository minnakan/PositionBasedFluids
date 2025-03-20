#include "PBFSystem.h"
#include "PBFComputeSystem.h"
#include <iostream>
#include <random>

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

    restDensity = 125.0f;

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
    case SceneType::DropBlock:
        dropWaterBlock();
        break;
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

    const int numSubsteps = 4;
    const float subDt = dt / numSubsteps;
    float warmupProgress = std::min(1.0f, frameCount / (float)warmupFrames);
    glm::vec4 scaledGravity = gravity * warmupProgress;

    for (int subStep = 0; subStep < numSubsteps; ++subStep) {
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

    computeSystem->downloadParticles(particles);

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
    const float damHeight = 40.0f;
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