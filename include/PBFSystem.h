#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <Camera.h> 
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

    bool computeSystemInitialized;
    PBFComputeSystem* computeSystem;

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

    void renderParticlesGPU(Camera& camera, int screenWidth, int screenHeight);

    void toggleGPURenderingMode() { useGPURendering = !useGPURendering; }
    bool isUsingGPURendering() const { return useGPURendering; }

    void step();

private:
    void initializeComputeSystem();
    void initializeGPURendering();

    unsigned int lastRenderedParticleCount = 0;

    
    
    int frameCount;
    int warmupFrames;

    bool waveModeActive;
    float waveTime;
    float waveAmplitude;
    float waveFrequency;

    bool useGPURendering = false;
    unsigned int gpuRenderVAO = 0;
    unsigned int gpuRenderVBO = 0;
    unsigned int gpuShaderProgram = 0;

};