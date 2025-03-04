#pragma once
#include <vector>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <unordered_map>

// A small struct to hold per-particle data
struct Particle
{
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 predictedPosition;
    float density;
    float lambda;
    glm::vec3 deltaP;

    // For debug visualization
    glm::vec3 color;
};

// Custom hash function for 3D grid cells
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

    // PBF parameters
    float restDensity = 1000.0f;
    float particleRadius = 0.1f;
    float h = 0.2f;      // kernel radius (smoothing length)
    int solverIterations = 4;
    float relaxationFactor = 0.5f; // relaxation for constraint projection
    float artificalPressureK = 0.1f; // artificial pressure strength
    float artificialPressureDeltaQ = 0.2f * 0.1f; // 0.2 * particleRadius
    int artificialPressureN = 4; // artificial pressure power
    float vorticityStrength = 0.1f; // vorticity confinement strength
    float xsphC = 0.01f; // XSPH viscosity factor

    // Collision parameters
    float boundaryDamping = 0.5f;
    float epsilon = 0.0001f; // small value to prevent division by zero

    // Boundaries (for a simple box)
    glm::vec3 minBoundary = glm::vec3(-5.0f, 0.0f, -5.0f);
    glm::vec3 maxBoundary = glm::vec3(5.0f, 10.0f, 5.0f);

    // The container of all particles
    std::vector<Particle> particles;

    // Grid for spatial partitioning (neighbor search)
    std::unordered_map<glm::ivec3, std::vector<int>, GridCellHash> grid;

    // Basic constructor
    PBFSystem();

    // Initialize or reset
    void initScene();

    // Main simulation step
    void step();

private:
    // Helper methods for PBF
    void predictPositions();
    void buildGrid();
    std::vector<int> getNeighbors(const glm::vec3& position);
    void computeDensities(std::vector<std::vector<int>>& neighbors);
    void computeLambdas(std::vector<std::vector<int>>& neighbors);
    void computePositionCorrections(std::vector<std::vector<int>>& neighbors);
    void applyPositionCorrections();
    void updateVelocities();
    void applyVorticityConfinement(std::vector<std::vector<int>>& neighbors);
    void applyXSPHViscosity(std::vector<std::vector<int>>& neighbors);

    // SPH kernel functions
    float kernelPoly6(float r, float h);
    glm::vec3 kernelSpikyGrad(const glm::vec3& r, float h);

    // Boundary handling
    void enforceBoxBoundaries(bool predictedPos = false);
};