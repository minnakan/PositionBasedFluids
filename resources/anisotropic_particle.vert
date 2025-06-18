#version 430 core

layout(location = 0) in uint particleId;

struct Particle {
    vec3 position;
    float padding1;
    vec3 velocity;
    float padding2;
    vec3 predictedPos;
    float padding3;
    vec3 color;
    float padding4;
    float density;
    float lambda;
    vec2 padding5;
};

// Original particle data
layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

// Smoothed centers from Laplacian smoothing
layout(std430, binding = 1) readonly buffer SmoothedCenters {
    vec4 smoothedCenters[];
};

// Anisotropy matrices from PCA
layout(std430, binding = 2) readonly buffer Anisotropy {
    mat4 Gs[];
};

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;
uniform float particleRadius;
uniform vec3 viewPos;
uniform vec3 lightPos;

out vec3 FragPos;
out vec3 Normal;
out vec3 ParticleColor;
out vec3 ViewDir;
out vec3 LightDir;
out vec4 ClipSpace;
out mat3 TBN;
out float Radius;

// Constants for ellipsoid rendering
const float PI = 3.14159265359;
const uint ELLIPSOID_SLICES = 8;
const uint ELLIPSOID_STACKS = 8;

void main() {
    // Get particle data
    vec3 pos = particles[particleId].position;
    vec3 center = smoothedCenters[particleId].xyz;
    vec3 color = particles[particleId].color;
    mat4 G = Gs[particleId];
    
    // Extract anisotropy transformation (first 3x3 part of G)
    mat3 anisotropy = mat3(G[0].xyz, G[1].xyz, G[2].xyz);
    
    // Calculate basis vectors for the ellipsoid
    vec3 xAxis = anisotropy[0] * particleRadius * 2.0;
    vec3 yAxis = anisotropy[1] * particleRadius * 2.0;
    vec3 zAxis = anisotropy[2] * particleRadius * 2.0;
    
    // Scale the point size based on the anisotropy
    float maxScale = max(length(xAxis), max(length(yAxis), length(zAxis)));
    
    // Calculate view-dependent point size
    float dist = length(viewPos - center);
    gl_PointSize = max(5.0, 500.0 * maxScale / dist);
    
    // Create TBN matrix for normal mapping in fragment shader
    vec3 T = normalize(xAxis);
    vec3 B = normalize(yAxis);
    vec3 N = normalize(zAxis);
    TBN = mat3(T, B, N);
    
    // Pass the radius for fragment shader calculations
    Radius = particleRadius;
    
    // Calculate lighting vectors
    vec3 worldPos = (model * vec4(center, 1.0)).xyz;
    FragPos = worldPos;
    ViewDir = normalize(viewPos - worldPos);
    LightDir = normalize(lightPos - worldPos);
    ParticleColor = color;
    
    // Pass clip space position
    ClipSpace = projection * view * model * vec4(center, 1.0);
    gl_Position = ClipSpace;
    
    // We'll reconstruct the normal in the fragment shader based on
    // the ellipsoid shape and gl_PointCoord
    Normal = vec3(0.0, 0.0, 1.0); // Placeholder
}