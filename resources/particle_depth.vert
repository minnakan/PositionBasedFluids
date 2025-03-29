#version 430 core

// Input is vertex ID
layout (location = 0) in uint vertexID;

// Uniform matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float particleRadius;

// For depth shader, get position from the SSBO
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

layout(std430, binding = 1) readonly buffer ParticleBuffer {
    Particle particles[];
};

// Output to fragment shader
out float Depth;

void main()
{
    // Get particle data from the SSBO using vertexID
    Particle particle = particles[vertexID];
    
    // Transform position to view space
    vec4 worldPos = model * vec4(particle.position, 1.0);
    vec4 viewPos = view * worldPos;
    
    // Calculate the final position
    gl_Position = projection * viewPos;
    
    // Pass linear depth to fragment shader (not normalized)
    // This gives better depth resolution
    Depth = -viewPos.z; // Negate because view space Z is negative for objects in front of camera
    
    // Calculate the point size based on distance from camera
    // Use a quadratic attenuation for more realistic sizing
    float dist = length(viewPos.xyz);
    float scale = 500.0 / (dist);
    
    // Apply min/max size constraints
    gl_PointSize = clamp(particleRadius * scale, 1.0, 50.0);
}