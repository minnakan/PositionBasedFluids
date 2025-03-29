#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;
uniform mat4 projection;
uniform mat4 view;
uniform vec2 screenSize;

// Parameters for normal reconstruction
uniform float normalStrength = 1;

void main()
{
    // Sample depth at current fragment
    float depth = texture(depthMap, TexCoords).r;
    
    // Check if this is a background pixel (depth at or near 1.0)
    if (depth >= 0.99) {
        // Output transparent pixel for background
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    
    // Calculate pixel size for sampling neighbors
    vec2 pixelSize = 1.0 / screenSize;
    
    // Sample neighboring depths (using a 3x3 Sobel filter pattern)
    float depthLeft = texture(depthMap, TexCoords - vec2(pixelSize.x, 0.0)).r;
    float depthRight = texture(depthMap, TexCoords + vec2(pixelSize.x, 0.0)).r;
    float depthTop = texture(depthMap, TexCoords + vec2(0.0, pixelSize.y)).r;
    float depthBottom = texture(depthMap, TexCoords - vec2(0.0, pixelSize.y)).r;
    
    // Skip background neighbors
    if (depthLeft >= 0.99) depthLeft = depth;
    if (depthRight >= 0.99) depthRight = depth;
    if (depthTop >= 0.99) depthTop = depth;
    if (depthBottom >= 0.99) depthBottom = depth;
    
    // Calculate depth gradients
    float dx = (depthRight - depthLeft) * normalStrength;
    float dy = (depthTop - depthBottom) * normalStrength;
    
    // Convert depth gradients to view-space normal
    // The z component is negative because the depth increases away from the camera
    vec3 normal = normalize(vec3(dx, dy, -1.0 * (abs(dx) + abs(dy))));
    
    // Transform to world space if needed
    // normal = mat3(transpose(inverse(view))) * normal;
    
    // Output normal in RGB (transform from [-1,1] to [0,1] range)
    FragColor = vec4(normal * 0.5 + 0.5, 1.0);
}