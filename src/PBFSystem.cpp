#include "PBFSystem.h"
#include "PBFComputeSystem.h"
#include <iostream>
#include <random>

PBFSystem::PBFSystem()
{
	// Default simulation parameters - Should be set before initScene()
    dt = 0.033f;
    gravity = glm::vec4(0.0f, -9.81f, 0.0f, 0.0f);
    particleRadius = 0.1f;
    h = particleRadius * 2.0f;  // Smoothing length

    minBoundary = glm::vec4(-2.5f, 0.0f, -5.0f, 0.0f);
    maxBoundary = glm::vec4(2.5f, 5.0f, 5.0f, 0.0f);

	cellSize = 0.2f;
	maxParticlesPerCell = 1024;

    restDensity = 1000.0f;

    computeSystem = nullptr;
    computeSystemInitialized = false;
}

PBFSystem::~PBFSystem()
{
    delete computeSystem;
}

void PBFSystem::initScene()
{
    // Reset
    particles.clear();

    // Configurable percentage of boundary to use (50%)
    float boundaryUsagePercent = 0.5f;

    // Fixed particle counts as requested
    const int numX = 42, numY = 42, numZ = 42;

    // Calculate available space within boundaries
    glm::vec3 minPos = glm::vec3(minBoundary.x, minBoundary.y, minBoundary.z);
    glm::vec3 maxPos = glm::vec3(maxBoundary.x, maxBoundary.y, maxBoundary.z);
    glm::vec3 totalSpace = maxPos - minPos;

    // Calculate reduced space based on the usage percentage
    glm::vec3 halfUnusedSpace = totalSpace * (1.0f - boundaryUsagePercent) * 0.5f;
    glm::vec3 reducedMinPos = minPos + halfUnusedSpace;
    glm::vec3 reducedMaxPos = maxPos - halfUnusedSpace;

    // Account for particle radius to keep particles fully within boundaries
    reducedMinPos += glm::vec3(particleRadius);
    reducedMaxPos -= glm::vec3(particleRadius);

    // Available space for particles
    glm::vec3 availableSpace = reducedMaxPos - reducedMinPos;

    // Calculate spacing based on available space and particle count
    glm::vec3 spacing = glm::vec3(availableSpace.x / (float)numX,availableSpace.y / (float)numY,availableSpace.z / (float)numZ);

    // Use minimum spacing value for all dimensions to keep particles spherical
    float minSpacing = std::min(std::min(spacing.x, spacing.y), spacing.z);

    // Calculate starting position to center the block
    glm::vec3 blockSize = glm::vec3(numX, numY, numZ) * minSpacing;
    glm::vec3 start = reducedMinPos + (availableSpace - blockSize) * 0.5f;

    // Move block up for a more interesting demo
    // Keep the vertical position higher but within the reduced space
    start.y = reducedMinPos.y + availableSpace.y * 0.6f;

    // Ensure start position is valid
    start = glm::max(start, reducedMinPos);

    std::cout << "[PBFSystem] Using " << (boundaryUsagePercent * 100.0f) << "% of boundary space\n";
    std::cout << "[PBFSystem] Creating particles: " << numX << "x" << numY << "x" << numZ<< " (" << (numX * numY * numZ) << " total)\n";
    std::cout << "[PBFSystem] Block starts at: " << start.x << ", " << start.y << ", " << start.z << "\n";
    std::cout << "[PBFSystem] Using spacing: " << minSpacing << "\n";

    // Random jitter for natural arrangement
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.02f, 0.02f);

    // Create particles
    for (int x = 0; x < numX; ++x) {
        for (int y = 0; y < numY; ++y) {
            for (int z = 0; z < numZ; ++z) {
                Particle p;
                p.position = glm::vec3(
                    start.x + x * minSpacing + jitter(gen) * minSpacing * 0.1f,
                    start.y + y * minSpacing + jitter(gen) * minSpacing * 0.1f,
                    start.z + z * minSpacing + jitter(gen) * minSpacing * 0.1f
                );

                // Final safety check to ensure position is within full boundaries
                p.position.x = std::clamp(p.position.x, minBoundary.x + particleRadius, maxBoundary.x - particleRadius);
                p.position.y = std::clamp(p.position.y, minBoundary.y + particleRadius, maxBoundary.y - particleRadius);
                p.position.z = std::clamp(p.position.z, minBoundary.z + particleRadius, maxBoundary.z - particleRadius);

                p.padding1 = 0.0f;
                p.velocity = glm::vec3(0.0f);
                p.padding2 = 0.0f;
                p.predictedPosition = p.position;
                p.padding3 = 0.0f;

                // Color by height ratio
                float heightRatio = (float)y / numY;
                if (heightRatio < 0.33f) {
                    // Blue-ish
                    p.color = glm::vec3(0.1f, 0.2f, 0.9f);
                }
                else if (heightRatio < 0.66f) {
                    // Green-ish
                    p.color = glm::vec3(0.1f, 0.9f, 0.2f);
                }
                else {
                    // Red-ish
                    p.color = glm::vec3(0.9f, 0.2f, 0.1f);
                }
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

    // Update simulation parameters in GPU
    computeSystem->updateSimulationParams(dt, gravity, particleRadius, h, minBoundary, maxBoundary,cellSize,maxParticlesPerCell,restDensity);
    // Run GPU step
    computeSystem->step();

    // Download to CPU so CPU can also see the updated positions
    computeSystem->downloadParticles(particles);
}

void PBFSystem::initializeComputeSystem()
{
    if (!computeSystem) {
        computeSystem = new PBFComputeSystem();
    }

    // Some max capacity
    const unsigned int MAX_PARTICLES = 1000000;
    bool success = computeSystem->initialize(MAX_PARTICLES,dt,gravity,particleRadius,h,minBoundary,maxBoundary,cellSize,maxParticlesPerCell,restDensity);

    if (success) {
        computeSystemInitialized = true;
        std::cout << "[PBFSystem] GPU compute system initialized\n";
        computeSystem->updateSimulationParams(dt, gravity, particleRadius, h, minBoundary, maxBoundary, cellSize, maxParticlesPerCell,restDensity);
    }
    else {
        std::cerr << "[PBFSystem] Failed to initialize GPU compute system\n";
    }
}
