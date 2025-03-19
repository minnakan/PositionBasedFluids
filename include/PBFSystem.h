#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "PBFComputeSystem.h"

enum class SceneType {
    DamBreak = 0,            
    WaterContainer = 1,
    DropBlock = 2
};

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
    float vorticityEpsilon;
    float xsphViscosityCoeff;
    bool enableDebugInfo;
    SceneType currentScene;

    // Public particle data for rendering
    std::vector<Particle> particles;

    PBFSystem();
    ~PBFSystem();

    // Initialize the simulation with a simple demo scene
    void initScene(SceneType sceneType = SceneType::DamBreak);
    void createDamBreakScene();
    void createWaterContainerScene();
    void dropWaterBlock();

    // Advance the simulation by one time step
    void step();

private:
    // Initialize the GPU compute system
    void initializeComputeSystem();

    // Flags and handles
    bool computeSystemInitialized;
    PBFComputeSystem* computeSystem;
    int frameCount;
    int warmupFrames;
    


};