#pragma once
#include <vector>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <unordered_map>

// A simple struct to hold per-particle data
struct Particle
{
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 predictedPosition;
    glm::vec3 color;
};

// Custom hash function for 3D grid cells (for future neighbor search)
struct GridCellHash {
    std::size_t operator()(const glm::ivec3& k) const {
        return ((k.x * 73856093) ^ (k.y * 19349663) ^ (k.z * 83492791)) % 10000000;
    }
};

class PBFSystem
{
public:
    // Simulation parameters
    float dt = 0.016f;   // time step
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);

    // Particle properties
    float particleRadius = 0.08f;

    // Collision parameters
    float boundaryDamping = 0.5f;

    // Boundaries (for a simple box)
    glm::vec3 minBoundary = glm::vec3(-4.0f, 0.0f, -4.0f);
    glm::vec3 maxBoundary = glm::vec3(4.0f, 10.0f, 4.0f);

    // The container of all particles
    std::vector<Particle> particles;

    // Basic constructor
    PBFSystem();

    // Initialize or reset
    void initScene();

    // Main simulation step
    void step();

private:
    // Simulation steps
    void applyExternalForces();
    void predictPositions();
    void enforceBoxBoundaries(bool predictedPos = false);
    void updateVelocities();
};