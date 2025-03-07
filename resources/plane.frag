#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoord;

uniform vec3 planeColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    // Basic lighting calculation
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(Normal, lightDir), 0.2); // Minimum ambient of 0.2
    
    // Add a grid pattern
    float gridSize = 1.0;
    float gridWidth = 0.05;
    
    // Create grid lines
    float gridX = abs(mod(TexCoord.x, gridSize) - gridSize * 0.5);
    float gridZ = abs(mod(TexCoord.y, gridSize) - gridSize * 0.5);
    
    // Determine if we're on a grid line
    float grid = step(gridSize * 0.5 - gridWidth, gridX) + step(gridSize * 0.5 - gridWidth, gridZ);
    grid = clamp(grid, 0.0, 1.0);
    
    // Darken color where grid lines are
    vec3 color = mix(planeColor, planeColor * 0.7, grid);
    
    // Apply lighting
    color = color * diff;
    
    FragColor = vec4(color, 1.0);
}