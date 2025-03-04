#include "PBFSystem.h"
#include <iostream>

PBFSystem::PBFSystem()
{
    // You might set default parameters here.
}

void PBFSystem::initScene()
{
    particles.clear();

    // Example: add a simple block of particles dropping from above
    // For now, just a small NxN block to see them fall.
    const int N = 8;
    float spacing = 0.5f;
    glm::vec3 start(0.0f, 5.0f, 0.0f);

    for (int x = 0; x < N; ++x)
    {
        for (int y = 0; y < N; ++y)
        {
            Particle p;
            p.position = start + glm::vec3(x * spacing, y * spacing, 0.0f);
            p.velocity = glm::vec3(0.0f);
            particles.push_back(p);
        }
    }

}

// Our simplest time-integration for Step 1
void PBFSystem::step()
{
    // (1) Basic gravity integration
    for (auto& p : particles)
    {
        p.velocity += gravity * dt;
    }
    // (2) Update position
    for (auto& p : particles)
    {
        p.position += p.velocity * dt;
    }

    // (3) Plane collision at y=0
    float groundY = 0.0f;
    float restitution = 0.5f; // 0=sticky, >0 = bounce

    for (auto& p : particles)
    {
        if (p.position.y < groundY)
        {
            // Place it on the plane
            p.position.y = groundY;
            // Invert or zero out the normal-component of velocity
            // so it doesn't keep going below the plane
            p.velocity.y *= -restitution;
        }
    }
}
