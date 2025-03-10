#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "PBFComputeSystem.h"

class PBFSystem {
public:
    // Public simulation parameters for easy access
    float dt;
    float boundaryDamping;
    glm::vec4 gravity;
    float particleRadius;
    float h;  // Smoothing length
    glm::vec4 minBoundary;
    glm::vec4 maxBoundary;
    float cellSize;
    unsigned int maxParticlesPerCell;
    float restDensity;
    bool enableDebugInfo;

    // Public particle data for rendering
    std::vector<Particle> particles;

    PBFSystem();
    ~PBFSystem();

    // Initialize the simulation with a simple demo scene
    void initScene();

    // Advance the simulation by one time step
    void step();

private:
    // Initialize the GPU compute system
    void initializeComputeSystem();

    // Flags and handles
    bool computeSystemInitialized;
    PBFComputeSystem* computeSystem;
};