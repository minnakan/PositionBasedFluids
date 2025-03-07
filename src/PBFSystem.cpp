#include "PBFSystem.h"
#include <iostream>
#include <random>
#include <glm/gtx/norm.hpp>

PBFSystem::PBFSystem()
{
    // Basic initialization of simulation parameters
    dt = 0.016f;  // 60 fps
    boundaryDamping = 0.5f;
}

void PBFSystem::initScene()
{
    particles.clear();

    // Create a block of particles for simulation
    const int numX = 12;  // Increased slightly for same overall volume
    const int numY = 12;  // Increased slightly for same overall volume 
    const int numZ = 12;  // Increased slightly for same overall volume
    float spacing = particleRadius * 2.1f;  // Slightly increased relative spacing

    // Start position - higher up for more visible gravity effect
    glm::vec3 start(-numX * spacing * 0.5f, 6.0f, -numZ * spacing * 0.5f);

    // Add some random jitter for natural looking behavior
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> jitter(-0.02f, 0.02f);

    // Add particles in a block formation
    for (int x = 0; x < numX; ++x)
    {
        for (int y = 0; y < numY; ++y)
        {
            for (int z = 0; z < numZ; ++z)
            {
                Particle p;
                p.position = start + glm::vec3(
                    x * spacing + jitter(gen),
                    y * spacing + jitter(gen),
                    z * spacing + jitter(gen)
                );
                p.predictedPosition = p.position;
                p.velocity = glm::vec3(0.0f);

                // Three-color system based on height (y position)
                // Bottom third: Blue
                // Middle third: Green
                // Top third: Red
                float heightRatio = (float)y / numY; // 0.0 to 1.0

                if (heightRatio < 0.33f) {
                    // Bottom third - Blue
                    p.color = glm::vec3(0.1f, 0.2f, 0.9f);
                }
                else if (heightRatio < 0.66f) {
                    // Middle third - Green
                    p.color = glm::vec3(0.1f, 0.9f, 0.2f);
                }
                else {
                    // Top third - Red
                    p.color = glm::vec3(0.9f, 0.2f, 0.1f);
                }

                particles.push_back(p);
            }
        }
    }

    std::cout << "Created " << particles.size() << " particles" << std::endl;
}

void PBFSystem::step()
{
    // Apply external forces and predict positions
    applyExternalForces();
    predictPositions();

    // Handle collisions with boundaries
    enforceBoxBoundaries(true);  // Handle for predicted positions

    // Update velocities based on position changes
    updateVelocities();

    // Final boundary check on actual positions
    enforceBoxBoundaries(false);
}

void PBFSystem::applyExternalForces()
{
    for (auto& p : particles)
    {
        p.velocity += gravity * dt;
    }
}

void PBFSystem::predictPositions()
{
    for (auto& p : particles)
    {
        // Limit velocity to prevent explosive behavior
        float maxSpeed = 10.0f;
        float speedSq = glm::length2(p.velocity);
        if (speedSq > maxSpeed * maxSpeed) {
            p.velocity = glm::normalize(p.velocity) * maxSpeed;
        }

        // Predict new position
        p.predictedPosition = p.position + p.velocity * dt;
    }
}

void PBFSystem::enforceBoxBoundaries(bool predictedPos)
{
    for (auto& p : particles)
    {
        glm::vec3& pos = predictedPos ? p.predictedPosition : p.position;

        // Check and enforce boundaries in each dimension
        for (int dim = 0; dim < 3; ++dim)
        {
            // Bottom/lower boundary (floor)
            if (pos[dim] < minBoundary[dim] + particleRadius) {
                pos[dim] = minBoundary[dim] + particleRadius;

                if (!predictedPos) {
                    // Reflect velocity with damping
                    p.velocity[dim] *= -boundaryDamping;
                }
            }
            // Upper boundaries
            else if (pos[dim] > maxBoundary[dim] - particleRadius) {
                pos[dim] = maxBoundary[dim] - particleRadius;

                if (!predictedPos) {
                    // Reflect velocity with damping
                    p.velocity[dim] *= -boundaryDamping;
                }
            }
        }
    }
}

void PBFSystem::updateVelocities()
{
    for (auto& p : particles)
    {
        // Compute velocity from position change
        p.velocity = (p.predictedPosition - p.position) / dt;

        // Update position
        p.position = p.predictedPosition;
    }
}