#pragma once

#include <vector>
#include <glm/glm.hpp>

class Camera;
class PBFSystem;
class ComputeShader;
class Shader;
class EllipsoidalGridSearch;
class AnisotropicSurfaceReconstruction;

class WaterRenderer {
public:
    WaterRenderer();
    ~WaterRenderer();

    bool initialize(int width, int height, float particleRadius);
    void renderFluid(const PBFSystem& pbf, const Camera& camera, const glm::vec3& lightPos);
    void cleanup();

    enum class RenderMode {
        ANISOTROPIC_PARTICLES,
        SURFACE_ONLY,
        PARTICLES_AND_SURFACE
    };

    RenderMode renderMode;

private:
    // Particle rendering
    void createParticleVAO();
    void renderAnisotropicParticles(const PBFSystem& pbf, const Camera& camera, const glm::vec3& lightPos);
    void computeAnisotropicParameters(const PBFSystem& pbf);

    // Surface rendering
    //void createSurfaceVAO();
    //void updateSurfaceBuffers();
    void renderSurface(const Camera& camera, const glm::vec3& lightPos);
    //void reconstructFluidSurface(const PBFSystem& pbf);

    // Dimensions and particle scale
    int screenWidth;
    int screenHeight;
    float particleRadius;
    unsigned int maxParticles;

    // Buffers and shaders
    unsigned int particleVAO, particleVBO;
    unsigned int surfaceVAO, surfaceVBO, surfaceEBO;
    unsigned int surfaceParticleBuffer, smoothedCentersBuffer, anisotropyBuffer;

    Shader* particleShader;
    Shader* surfaceShader;
    ComputeShader* surfaceDetectionShader;
    ComputeShader* smoothCenterShader;
    ComputeShader* anisotropyShader;

    // Surface data
    std::vector<glm::vec3> surfaceVertices;
    std::vector<glm::vec3> surfaceNormals;
    std::vector<unsigned int> surfaceIndices;
    unsigned int surfaceVertexCount;
    unsigned int surfaceIndexCount;
};
