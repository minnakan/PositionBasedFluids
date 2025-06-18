#version 430 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 ParticleColor;
in vec3 ViewDir;
in vec3 LightDir;
in vec4 ClipSpace;
in mat3 TBN;
in float Radius;

uniform vec3 lightPos;
uniform vec3 viewPos;

void main() {
    // Convert point coordinates to [-1,1] range
    vec2 coord = 2.0 * gl_PointCoord - 1.0;
    
    // Ellipsoid ray-casting
    // We're creating a 3D normal from the 2D gl_PointCoord
    // This gives us the appearance of a 3D ellipsoid
    float len = dot(coord, coord);
    if (len > 1.0) {
        discard; // Outside the circle
    }
    
    // Calculate Z coordinate on the sphere
    float z = sqrt(1.0 - len);
    
    // Create a normal in local space
    vec3 localNormal = normalize(vec3(coord.x, coord.y, z));
    
    // Transform the normal using the TBN matrix to get the world-space normal
    // This applies the anisotropy transformation
    vec3 worldNormal = normalize(TBN * localNormal);
    
    // Lighting calculations
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * ParticleColor;
    
    // Diffuse
    float diff = max(dot(worldNormal, LightDir), 0.0);
    vec3 diffuse = diff * ParticleColor;
    
    // Specular
    float specularStrength = 0.7;
    vec3 halfwayDir = normalize(LightDir + ViewDir);
    float spec = pow(max(dot(worldNormal, halfwayDir), 0.0), 64.0);
    vec3 specular = specularStrength * spec * vec3(1.0);
    
    // Fresnel effect for water-like appearance
    float fresnel = 0.2 + 0.8 * pow(1.0 - max(dot(worldNormal, ViewDir), 0.0), 5.0);
    
    // Water color tint
    vec3 waterColor = vec3(0.2, 0.5, 0.8);
    
    // Depth-based fog effect for better water appearance
    // Mix particle color with water color based on camera distance
    float fogFactor = min(length(viewPos - FragPos) / 40.0, 1.0);
    vec3 tintedColor = mix(ParticleColor, waterColor, 0.7);
    
    // Final color
    vec3 result = (ambient + diffuse) * tintedColor + specular;
    
    // Apply fresnel and depth effects
    result = mix(result, waterColor * 1.5, fresnel * 0.5);
    
    // Add subtle depth-based transparency
    float alpha = mix(0.8, 0.95, z);
    
    FragColor = vec4(result, alpha);
}