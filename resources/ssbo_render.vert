#version 430 core

// Input is just a vertex ID
layout (location = 0) in uint vertexID;

// Uniform matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float particleRadius;

// Output to fragment shader
out vec3 FragPos;
out vec3 Color;

// Particle structure - must match the CPU and compute shader definition
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

// Bind the particle SSBO to binding point 1 (matching your compute shaders)
layout(std430, binding = 1) readonly buffer ParticleBuffer {
    Particle particles[];
};

void main()
{
    // Get the particle data directly from the SSBO
    Particle particle = particles[vertexID];
    
    // Transform position to world space for fragment shader
    FragPos = vec3(model * vec4(particle.position, 1.0));
    
    // Pass color directly to fragment shader
    Color = particle.color;
    
    // Transform position to clip space
    gl_Position = projection * view * model * vec4(particle.position, 1.0);
    
    float dist = length((view * model * vec4(particle.position, 1.0)).xyz);
    float scale = 500.0 / dist;
    
    // Apply min/max size constraints
    gl_PointSize = clamp(particleRadius * scale, 1.0, 30.0);
}