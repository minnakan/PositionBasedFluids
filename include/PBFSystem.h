#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "PBFComputeSystem.h"

class PBFSystem {
public:
    // Public simulation parameters for easy access
    float dt;
    float boundaryDamping;
    glm::vec3 gravity;
    float particleRadius;
    float h;  // Smoothing length
    glm::vec3 minBoundary;
    glm::vec3 maxBoundary;
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