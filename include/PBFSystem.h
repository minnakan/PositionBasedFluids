#pragma once
#include <vector>
#include <glm/glm.hpp>

// A small struct to hold per-particle data.
// For now, only position & velocity. We’ll expand this later.
struct Particle
{
    glm::vec3 position;
    glm::vec3 velocity;

    // (Optionally store accumulated force, mass, etc.)
    // float mass;
};

class PBFSystem
{
public:
    // Simulation parameters (tweak as needed).
    float dt = 0.016f;   // time step
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);

    // The container of all particles.
    std::vector<Particle> particles;

    // Basic constructor: you might want to pass #particles, domain, etc.
    PBFSystem();

    // Initialize or reset
    void initScene();

    // 1) Update velocities with external forces
    // 2) Predict new positions
    // 3) (No constraints yet in Step 1)
    // 4) Update final positions & velocities (trivial so far)
    void step();

    // Will fill in neighbor search, constraint solves, etc. in later steps

private:
    // e.g. helper methods...
};
