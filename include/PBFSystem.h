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
    float dt;
    float boundaryDamping;
    glm::vec4 gravity;
    float particleRadius;
    float h;  
    glm::vec4 minBoundary;
    glm::vec4 maxBoundary;
    float cellSize;
    unsigned int maxParticlesPerCell;
    float restDensity;
    float vorticityEpsilon;
    float xsphViscosityCoeff;
    bool enableDebugInfo;
    SceneType currentScene;

    glm::vec4 originalMinBoundary;

    std::vector<Particle> particles;

    PBFSystem();
    ~PBFSystem();

    void initScene(SceneType sceneType = SceneType::DamBreak);
    void createDamBreakScene();
    void createWaterContainerScene();
    void dropWaterBlock();

    void toggleWaveMode();
    bool isWaveModeActive() const { return waveModeActive; }

    void step();

private:
    void initializeComputeSystem();

    bool computeSystemInitialized;
    PBFComputeSystem* computeSystem;
    int frameCount;
    int warmupFrames;

    bool waveModeActive;
    float waveTime;
    float waveAmplitude;
    float waveFrequency;
};