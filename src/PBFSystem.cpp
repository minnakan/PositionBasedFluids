#include "PBFSystem.h"
#include "PBFComputeSystem.h"
#include <iostream>
#include <random>

PBFSystem::PBFSystem()
{
    dt = 0.016f;
    gravity = glm::vec4(0.0f, -9.81f, 0.0f, 0.0f);
    particleRadius = 0.1f;
    h = particleRadius * 2.0f;  // Smoothing length

    minBoundary = glm::vec4(-10.0f, 0.0f, -5.0f, 0.0f);
    maxBoundary = glm::vec4(10.0f, 10.0f, 5.0f, 0.0f);

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

    // Create a small block of particles
    const int numX = 25, numY = 25, numZ = 25;
    float spacing = particleRadius * 2.1f;
    glm::vec3 start(-numX * spacing * 0.5f, 6.0f, -numZ * spacing * 0.5f);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.02f, 0.02f);

    for (int x = 0; x < numX; ++x) {
        for (int y = 0; y < numY; ++y) {
            for (int z = 0; z < numZ; ++z) {
                Particle p;
                p.position = glm::vec3(
                    start.x + x * spacing + jitter(gen),
                    start.y + y * spacing + jitter(gen),
                    start.z + z * spacing + jitter(gen)
                );
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
    computeSystem->updateSimulationParams(dt, gravity, particleRadius, h, minBoundary, maxBoundary);
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
    const unsigned int MAX_PARTICLES = 100000;
    bool success = computeSystem->initialize(MAX_PARTICLES);

    if (success) {
        computeSystemInitialized = true;
        std::cout << "[PBFSystem] GPU compute system initialized\n";
        computeSystem->updateSimulationParams(
            dt, gravity, particleRadius, h, minBoundary, maxBoundary
        );
    }
    else {
        std::cerr << "[PBFSystem] Failed to initialize GPU compute system\n";
    }
}
