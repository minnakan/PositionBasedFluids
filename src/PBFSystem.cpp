#include "PBFSystem.h"
#include "PBFComputeSystem.h"
#include <iostream>
#include <random>

PBFSystem::PBFSystem()
{
	// Default simulation parameters - Should be set before initScene()
    dt = 0.016f;
    gravity = glm::vec4(0.0f, -9.81f * 1.0f, 0.0f, 0.0f);
    particleRadius = 0.2f;
    h = particleRadius * 2.5f;  // Smoothing length

    minBoundary = glm::vec4(-8.0f, 0.0f, -8.0f, 0.0f);
    maxBoundary = glm::vec4(8.0f, 100.0f, 8.0f, 0.0f);

	cellSize = h;
	maxParticlesPerCell = 64;

    restDensity = 250.0f;

    vorticityEpsilon = 0.01f;
    xsphViscosityCoeff = 0.01f;

    computeSystem = nullptr;
    computeSystemInitialized = false;

    frameCount = 0;
    warmupFrames = 0;

    currentScene = SceneType::DamBreak;
}

PBFSystem::~PBFSystem()
{
    delete computeSystem;
}

void PBFSystem::initScene(SceneType sceneType)
{
	
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
        //createWaterContainerScene();
        dropWaterBlock();
        break;
    default:
        std::cerr << "[PBFSystem] Unknown scene type, defaulting to dam break\n";
        frameCount = 0;
        particles.clear();
        createDamBreakScene();
        break;
    }


    // Initialize GPU system if needed
    if (!computeSystemInitialized) {
        initializeComputeSystem();
    }

    // Upload new particle set
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
    const int numSubsteps = 2;
    const float subDt = dt / numSubsteps;

    // Calculate warmup progress (0 to 1)
    float warmupProgress = std::min(1.0f, frameCount / (float)warmupFrames);

    // Scale gravity forces during warmup
    glm::vec4 scaledGravity = gravity * warmupProgress;

    for (int subStep = 0; subStep < numSubsteps; ++subStep) {
        computeSystem->updateSimulationParams(dt, scaledGravity, particleRadius, h, minBoundary, maxBoundary, cellSize, maxParticlesPerCell, restDensity,vorticityEpsilon,xsphViscosityCoeff);
        computeSystem->step();
    }

    // Download to CPU so CPU can also see the updated positions
    computeSystem->downloadParticles(particles);

    // Increment frame counter
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

void PBFSystem::createDamBreakScene()
{
    // Dam break parameters
    const float damWidth = 15.0f;
    const float damHeight = 50.0f;
    const float damDepth = 5.0f;

    // Place dam in left portion of container
    const float leftOffset = minBoundary.x + particleRadius * 3.0f;

    // Use conservative spacing (2.1x particle radius)
    const float spacing = particleRadius * 2.1f;

    // Calculate number of particles in each dimension
    const int numX = static_cast<int>(damWidth / spacing);
    const int numY = static_cast<int>(damHeight / spacing);
    const int numZ = static_cast<int>(damDepth / spacing);

    std::cout << "[PBFSystem] Creating dam break: " << numX << "x" << numY << "x" << numZ
        << " (" << (numX * numY * numZ) << " total particles)\n";
    std::cout << "[PBFSystem] Using spacing: " << spacing << " (radius: " << particleRadius << ")\n";

    // Tiny jitter to break symmetry (very small to maintain stability)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.001f, 0.001f);

    // Create particles in a rectangular dam formation
    for (int x = 0; x < numX; ++x) {
        for (int y = 0; y < numY; ++y) {
            for (int z = 0; z < numZ; ++z) {
                Particle p;

                // Position particles with exact spacing plus tiny jitter
                p.position = glm::vec3(
                    leftOffset + x * spacing + jitter(gen) * spacing * 0.01f,
                    minBoundary.y + particleRadius * 2.0f + y * spacing + jitter(gen) * spacing * 0.01f,
                    minBoundary.z + particleRadius * 3.0f + z * spacing + jitter(gen) * spacing * 0.01f
                );

                // Safety check boundary constraints
                p.position.x = std::clamp(p.position.x,
                    minBoundary.x + particleRadius * 1.5f,
                    maxBoundary.x - particleRadius * 1.5f);
                p.position.y = std::clamp(p.position.y,
                    minBoundary.y + particleRadius * 1.5f,
                    maxBoundary.y - particleRadius * 1.5f);
                p.position.z = std::clamp(p.position.z,
                    minBoundary.z + particleRadius * 1.5f,
                    maxBoundary.z - particleRadius * 1.5f);

                // Initialize with zero velocity
                p.padding1 = 0.0f;
                p.velocity = glm::vec3(0.0f);
                p.padding2 = 0.0f;
                p.predictedPosition = p.position;
                p.padding3 = 0.0f;

                // Color gradient from bottom (blue) to top (red)
                float heightRatio = static_cast<float>(y) / numY;
                p.color = glm::vec3(
                    heightRatio,           // R increases with height
                    0.2f,                  // G constant
                    1.0f - heightRatio     // B decreases with height
                );
                p.padding4 = 0.0f;

                particles.push_back(p);
            }
        }
    }

    std::cout << "[PBFSystem] Created " << particles.size() << " particles for dam break scene\n";
}

void PBFSystem::createWaterContainerScene()
{
    // Container parameters
    const float containerWidth = 14.0f;
    const float containerHeight = 14.0f;  // Lower height for just container
    const float containerDepth = 14.0f;

    // Use conservative spacing (2.1x particle radius)
    const float spacing = particleRadius * 2.1f;

    // Calculate offsets to center container
    const float centerX = (minBoundary.x + maxBoundary.x) * 0.5f;
    const float baseY = minBoundary.y + particleRadius * 2.0f;
    const float centerZ = (minBoundary.z + maxBoundary.z) * 0.5f;

    const float containerStartX = centerX - containerWidth * 0.5f;
    const float containerStartZ = centerZ - containerDepth * 0.5f;

    // Calculate number of particles in container
    const int containerNumX = static_cast<int>(containerWidth / spacing);
    const int containerNumY = static_cast<int>(containerHeight / spacing);
    const int containerNumZ = static_cast<int>(containerDepth / spacing);

    std::cout << "[PBFSystem] Creating water container: " << containerNumX << "x" << containerNumY << "x" << containerNumZ
        << " (" << (containerNumX * containerNumY * containerNumZ) << " particles)\n";

    // Prepare random jitter for breaking symmetry
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.001f, 0.001f);

    // Helper function to add a particle
    auto addParticle = [&](glm::vec3 pos, glm::vec3 color, glm::vec3 velocity = glm::vec3(0.0f)) {
        Particle p;

        // Position with jitter
        p.position = pos + glm::vec3(
            jitter(gen) * spacing * 0.01f,
            jitter(gen) * spacing * 0.01f,
            jitter(gen) * spacing * 0.01f
        );

        // Safety check boundary constraints
        p.position.x = std::clamp(p.position.x,
            minBoundary.x + particleRadius * 1.5f,
            maxBoundary.x - particleRadius * 1.5f);
        p.position.y = std::clamp(p.position.y,
            minBoundary.y + particleRadius * 1.5f,
            maxBoundary.y - particleRadius * 1.5f);
        p.position.z = std::clamp(p.position.z,
            minBoundary.z + particleRadius * 1.5f,
            maxBoundary.z - particleRadius * 1.5f);

        p.padding1 = 0.0f;
        p.velocity = velocity;
        p.padding2 = 0.0f;
        p.predictedPosition = p.position;
        p.padding3 = 0.0f;
        p.color = color;
        p.padding4 = 0.0f;

        particles.push_back(p);
        };

    // Create container particles
    for (int x = 0; x < containerNumX; ++x) {
        for (int y = 0; y < containerNumY; ++y) {
            for (int z = 0; z < containerNumZ; ++z) {
                glm::vec3 pos(
                    containerStartX + x * spacing,
                    baseY + y * spacing,
                    containerStartZ + z * spacing
                );

                // Color: nice blue gradient based on height
                float heightRatio = static_cast<float>(y) / containerNumY;
                glm::vec3 color(
                    0.0f,                        // R
                    0.3f + 0.2f * heightRatio,   // G increases with height
                    0.8f - 0.1f * heightRatio    // B slightly decreases with height
                );

                addParticle(pos, color);
            }
        }
    }

    std::cout << "[PBFSystem] Created " << particles.size() << " particles for water container scene\n";
}

void PBFSystem::dropWaterBlock()
{
    // We'll add a water block above the existing particles

    // Dropping block parameters
    const float dropBlockWidth = 8.0f;
    const float dropBlockHeight = 16.0f;
    const float dropBlockDepth = 8.0f;
    const float dropHeight = 40.0f;

    // Use conservative spacing (2.1x particle radius)
    const float spacing = particleRadius * 2.1f;

    // Calculate offsets to center entities
    const float centerX = (minBoundary.x + maxBoundary.x) * 0.5f;
    const float baseY = minBoundary.y + particleRadius * 2.0f;
    const float centerZ = (minBoundary.z + maxBoundary.z) * 0.5f;

    // Find the highest existing water particle to place block above it
    float highestY = baseY;
    for (const auto& particle : particles) {
        highestY = std::max(highestY, particle.position.y);
    }

    const float dropBlockStartX = centerX - dropBlockWidth * 0.5f;
    const float dropBlockStartY = highestY + dropHeight;
    const float dropBlockStartZ = centerZ - dropBlockDepth * 0.5f;

    // Calculate number of particles in drop block
    const int dropNumX = static_cast<int>(dropBlockWidth / spacing);
    const int dropNumY = static_cast<int>(dropBlockHeight / spacing);
    const int dropNumZ = static_cast<int>(dropBlockDepth / spacing);

    std::cout << "[PBFSystem] Adding water block: " << dropNumX << "x" << dropNumY << "x" << dropNumZ
        << " (" << (dropNumX * dropNumY * dropNumZ) << " particles)\n";

    // Prepare random jitter for breaking symmetry
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.001f, 0.001f);

    // Calculate particles in the existing scene before we add the block
    size_t existingParticles = particles.size();

    // Create drop block particles with a different color
    for (int x = 0; x < dropNumX; ++x) {
        for (int y = 0; y < dropNumY; ++y) {
            for (int z = 0; z < dropNumZ; ++z) {
                Particle p;

                // Position particles with exact spacing plus tiny jitter
                p.position = glm::vec3(
                    dropBlockStartX + x * spacing + jitter(gen) * spacing * 0.01f,
                    dropBlockStartY + y * spacing + jitter(gen) * spacing * 0.01f,
                    dropBlockStartZ + z * spacing + jitter(gen) * spacing * 0.01f
                );

                // Safety check boundary constraints
                p.position.x = std::clamp(p.position.x,
                    minBoundary.x + particleRadius * 1.5f,
                    maxBoundary.x - particleRadius * 1.5f);
                p.position.y = std::clamp(p.position.y,
                    minBoundary.y + particleRadius * 1.5f,
                    maxBoundary.y - particleRadius * 1.5f);
                p.position.z = std::clamp(p.position.z,
                    minBoundary.z + particleRadius * 1.5f,
                    maxBoundary.z - particleRadius * 1.5f);

                // Initialize with zero velocity
                p.padding1 = 0.0f;
                p.velocity = glm::vec3(0.0f);
                p.padding2 = 0.0f;
                p.predictedPosition = p.position;
                p.padding3 = 0.0f;

                // Color: red-orange gradient from bottom to top
                float heightRatio = static_cast<float>(y) / dropNumY;
                p.color = glm::vec3(
                    0.8f + 0.2f * heightRatio, // R
                    0.4f - 0.2f * heightRatio, // G
                    0.0f                       // B
                );
                p.padding4 = 0.0f;

                particles.push_back(p);
            }
        }
    }

    std::cout << "[PBFSystem] Added " << (particles.size() - existingParticles)
        << " particles for water block (total: " << particles.size() << ")\n";
}