#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float particleRadius;

out vec3 FragPos;  // Changed from ParticleColor to FragPos
out vec3 Color;    // Changed to Color to match fragment shader

void main()
{
    // Transform position to world space for fragment shader
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // Pass color directly to fragment shader
    Color = aColor;
    
    // Transform position to clip space
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    
    float dist = length((view * model * vec4(aPos, 1.0)).xyz);
    float scale = 500.0 / dist;
    // Apply min/max size constraints
    gl_PointSize = clamp(particleRadius * scale, 1.0, 30.0);
}