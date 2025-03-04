#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Color;

uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    // Calculate the sphere using point sprite technique
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float radius = dot(coord, coord);
    if (radius > 1.0) {
        discard; // Outside of the sphere
    }
    
    // Calculate 3D position on the sphere surface for lighting
    float z = sqrt(1.0 - radius);
    vec3 normal = vec3(coord.x, coord.y, z);
    
    // Base color from the vertex shader
    vec3 color = Color;
    
    // Lighting calculations
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * color;
    
    // Diffuse
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * color;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * vec3(1.0);
    
    // Combine lighting components
    vec3 result = ambient + diffuse + specular;
    
    // Add depth effect with darker shading toward the center
    float depthFade = mix(0.7, 1.0, z);
    result *= depthFade;
    
    // Add rim lighting effect at the edges of the sphere
    float rim = 1.0 - max(dot(viewDir, normal), 0.0);
    rim = smoothstep(0.6, 1.0, rim);
    vec3 rimColor = vec3(0.5, 0.7, 1.0) * rim * 0.5;
    
    // Final color with rim lighting
    result += rimColor;
    
    // Output with slight transparency for better blending
    FragColor = vec4(result, 0.95);
}