#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "ComputeShader.h"

// This struct must exactly match the GPU shader struct layout
struct Particle {
    glm::vec3 position;  // 0-11 bytes 
    float padding1;      // 12-15 bytes
    glm::vec3 velocity;  // 16-27 bytes
    float padding2;      // 28-31 bytes
    glm::vec3 predictedPosition; // 32-43 bytes
    float padding3;      // 44-47 bytes
    glm::vec3 color;     // 48-59 bytes
    float padding4;      // 60-63 bytes
};

// This struct must match the layout in your compute shader
struct SimParams {
    float dt;
    float padding0;
    glm::vec2 padding1;

    glm::vec4 gravity;
    float padding2;

    float particleRadius;
    float h;
    float pad1[2];

    glm::vec4 minBoundary;
    glm::vec4 maxBoundary;

    unsigned int numParticles;
    float cellSize;                                                                     
    unsigned int maxParticlesPerCell;
	float pad2;
};

class PBFComputeSystem {
public:
    PBFComputeSystem();
    ~PBFComputeSystem();

    bool initialize(unsigned int maxParticles);
    void uploadParticles(const std::vector<Particle>& particles);
    void downloadParticles(std::vector<Particle>& particles);
    void step();

	bool checkComputeShaderSupport();

    void updateSimulationParams(float dt,const glm::vec4& gravity,float particleRadius,float smoothingLength,const glm::vec4& minBoundary,const glm::vec4& maxBoundary);

private:
    void createBuffers(unsigned int maxParticles);
    void cleanup();

    ComputeShader* computeShader;
    GLuint simParamsUBO;
    GLuint particleSSBO;
    unsigned int numParticles;
    unsigned int maxParticles;
    SimParams params;
};