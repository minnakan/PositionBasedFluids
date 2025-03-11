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
}

PBFSystem::~PBFSystem()
{
    delete computeSystem;
}

void PBFSystem::initScene()
{
    // Reset
    particles.clear();
    frameCount = 0;  // Reset frame counter for warmup

    // Dam break parameters
    const float damWidth = 15.0f;
    const float damHeight = 50.0f;
    const float damDepth = 5.f;

    // Place dam in left portion of container
    const float leftOffset = minBoundary.x + particleRadius * 3.0f;

    // Use conservative spacing (2.1x particle radius)
    const float spacing = particleRadius * 2.1f;

    // Calculate number of particles in each dimension
    const int numX = static_cast<int>(damWidth / spacing);
    const int numY = static_cast<int>(damHeight / spacing);
    const int numZ = static_cast<int>(damDepth / spacing);

    std::cout << "[PBFSystem] Creating dam break: " << numX << "x" << numY << "x" << numZ<< " (" << (numX * numY * numZ) << " total particles)\n";
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

    std::cout << "[PBFSystem] Created " << particles.size() << " particles\n";

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

    // Update simulation parameters with scaled gravity
    //computeSystem->updateSimulationParams(dt, scaledGravity, particleRadius, h,minBoundary, maxBoundary, cellSize, maxParticlesPerCell, restDensity);

    // Run GPU step
    //computeSystem->step();

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
