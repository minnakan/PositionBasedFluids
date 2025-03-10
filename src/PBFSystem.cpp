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

    minBoundary = glm::vec4(-5.0f, 0.0f, -5.0f, 0.0f);
    maxBoundary = glm::vec4(5.0f, 20.0f, 5.0f, 0.0f);

	cellSize = h;
	maxParticlesPerCell = 64;

    restDensity = 1000.0f;

    computeSystem = nullptr;
    computeSystemInitialized = false;

    frameCount = 0;
    warmupFrames = 600;
}

PBFSystem::~PBFSystem()
{
    delete computeSystem;
}

void PBFSystem::initScene()
{
    // Reset
    particles.clear();

    // Use a smaller percentage of boundary to keep particles away from walls
    float boundaryUsagePercent = 0.25f;  // Reduced from 0.5

    // Fixed particle counts as requested
    const int numX = 20, numY = 20, numZ = 20;  // Reduced particle count for easier visualization

    // Calculate available space within boundaries
    glm::vec3 minPos = glm::vec3(minBoundary.x, minBoundary.y, minBoundary.z);
    glm::vec3 maxPos = glm::vec3(maxBoundary.x, maxBoundary.y, maxBoundary.z);
    glm::vec3 totalSpace = maxPos - minPos;

    // Calculate reduced space based on the usage percentage
    glm::vec3 halfUnusedSpace = totalSpace * (1.0f - boundaryUsagePercent) * 0.5f;
    glm::vec3 reducedMinPos = minPos + halfUnusedSpace;
    glm::vec3 reducedMaxPos = maxPos - halfUnusedSpace;

    // Add extra margin at the bottom to create a more visible stack
    reducedMinPos.y += totalSpace.y * 0.1f;  // Move starting position up

    // Account for particle radius to keep particles fully within boundaries
    reducedMinPos += glm::vec3(particleRadius * 2.0f);  // Double margin
    reducedMaxPos -= glm::vec3(particleRadius * 2.0f);  // Double margin

    // Available space for particles
    glm::vec3 availableSpace = reducedMaxPos - reducedMinPos;

    // Calculate spacing based on available space and particle count
    glm::vec3 spacing = glm::vec3(
        availableSpace.x / (float)numX,
        availableSpace.y / (float)numY,
        availableSpace.z / (float)numZ
    );

    // Use minimum spacing value for all dimensions to keep particles spherical
    float minSpacing = std::min(std::min(spacing.x, spacing.y), spacing.z);

    // Ensure minimum spacing is greater than 2x particle radius
    minSpacing = std::max(minSpacing, particleRadius * 2.1f);

    // Calculate starting position to center the block
    glm::vec3 blockSize = glm::vec3(numX, numY, numZ) * minSpacing;
    glm::vec3 start = reducedMinPos + (availableSpace - blockSize) * 0.5f;

    // Move block up for a more interesting demo
    // Keep the vertical position higher but within the reduced space
    start.y = reducedMinPos.y + availableSpace.y * 2.7f;  // Start higher up

    // Ensure start position is valid
    start = glm::max(start, reducedMinPos);

    std::cout << "[PBFSystem] Using " << (boundaryUsagePercent * 100.0f) << "% of boundary space\n";
    std::cout << "[PBFSystem] Creating particles: " << numX << "x" << numY << "x" << numZ << " (" << (numX * numY * numZ) << " total)\n";
    std::cout << "[PBFSystem] Block starts at: " << start.x << ", " << start.y << ", " << start.z << "\n";
    std::cout << "[PBFSystem] Using spacing: " << minSpacing << "\n";

    // Random jitter for natural arrangement
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.01f, 0.01f);  // Reduced jitter amount

    // Create particles in a cube formation
    for (int x = 0; x < numX; ++x) {
        for (int y = 0; y < numY; ++y) {
            for (int z = 0; z < numZ; ++z) {
                Particle p;
                p.position = glm::vec3(
                    start.x + x * minSpacing + jitter(gen) * minSpacing * 0.05f,
                    start.y + y * minSpacing + jitter(gen) * minSpacing * 0.05f,
                    start.z + z * minSpacing + jitter(gen) * minSpacing * 0.05f
                );

                // Final safety check to ensure position is within full boundaries
                p.position.x = std::clamp(p.position.x, minBoundary.x + particleRadius * 1.5f, maxBoundary.x - particleRadius * 1.5f);
                p.position.y = std::clamp(p.position.y, minBoundary.y + particleRadius * 1.5f, maxBoundary.y - particleRadius * 1.5f);
                p.position.z = std::clamp(p.position.z, minBoundary.z + particleRadius * 1.5f, maxBoundary.z - particleRadius * 1.5f);

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

    // Calculate warmup progress (0 to 1)
    float warmupProgress = std::min(1.0f, frameCount / (float)warmupFrames);

    // Scale gravity forces during warmup
    glm::vec4 scaledGravity = gravity * warmupProgress;

    // Update simulation parameters with scaled gravity
    computeSystem->updateSimulationParams(dt, scaledGravity, particleRadius, h,
        minBoundary, maxBoundary, cellSize, maxParticlesPerCell, restDensity);

    // Run GPU step
    computeSystem->step();

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
