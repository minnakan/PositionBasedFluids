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
    float density;       
    float lambda;        
    glm::vec2 padding5;
};

// This struct must match the layout in your compute shader
struct SimParams {
    // Group 1
    float dt;
    float _pad0;
    float _pad1;
    float _pad2;

    // Group 2
    glm::vec4 gravity;

    // Group 3
    float particleRadius;
    float h;
    float _pad3;
    float _pad4;

    // Group 4
    glm::vec4 minBoundary;
    glm::vec4 maxBoundary;

    // Group 5
    unsigned int numParticles;
    float cellSize;
    unsigned int maxParticlesPerCell;
    float restDensity;

    float vorticityEpsilon;
    float xsphViscosityCoeff;
    float _pad5;
    float _pad6;
};

class PBFComputeSystem {
public:
    PBFComputeSystem();
    ~PBFComputeSystem();

    bool initialize(unsigned int maxParticles, float dt, const glm::vec4& gravity, float particleRadius, float smoothingLength, const glm::vec4& minBoundary, const glm::vec4& maxBoundary, float cellSize,unsigned int maxParticlesPerCell,float restDensity, float vorticityEpsilon, float xsphViscosityCoeff);
    void uploadParticles(const std::vector<Particle>& particles);
    void downloadParticles(std::vector<Particle>& particles);
    void step();

	bool checkComputeShaderSupport();

    void applyExternalForces();
    void findNeighbors();
	void calculateDensity();
    void applyPositionUpdate();
    void applyVorticityViscosity();
    void updateVelocity();


    void updateSimulationParams(float dt,const glm::vec4& gravity,float particleRadius,float smoothingLength,const glm::vec4& minBoundary,const glm::vec4& maxBoundary, float cellSize, unsigned int maxParticlesPerCell, float restDensity, float vorticityEpsilon, float xsphViscosityCoeff);

private:
    void createBuffers(unsigned int maxParticles);
    void initializeGrid();
    //void bindBuffersForGridConstruction();

    void cleanup();

    ComputeShader* externalForcesShader;
    ComputeShader* constructGridShader;
    ComputeShader* clearGridShader;
    ComputeShader* densityShader;
    ComputeShader* positionUpdateShader;
    ComputeShader* vorticityViscosityShader;
    ComputeShader* velocityUpdateShader;

    GLuint simParamsUBO;
    GLuint particleSSBO;
    GLuint cellCountsBuffer; // Stores count of particles per cell
	GLuint cellParticlesBuffer; // Stores particle indices per cell
    unsigned int numParticles;
    unsigned int maxParticles;
    SimParams params;
};