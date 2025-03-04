#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float particleRadius = 0.05f;

out vec3 FragPos;
out vec3 Color;

void main()
{
    // Pass position and color to fragment shader
    FragPos = vec3(model * vec4(aPos, 1.0));
    Color = aColor;
    
    // Calculate point size based on distance to camera
    vec4 viewPos = view * vec4(FragPos, 1.0);
    float distance = -viewPos.z;
    
    // Scale point size based on distance and particle radius
    // We multiply by a factor to make spheres visible at distance
    gl_PointSize = particleRadius * 3000.0 / distance;
    
    // Output position
    gl_Position = projection * viewPos;
}