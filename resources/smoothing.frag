#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;
uniform sampler2D normalMap;
uniform vec2 screenSize;

// Parameters for curvature flow
uniform int smoothingIterations = 2;
uniform float smoothingStrength = 0.5;

void main()
{
    // Get pixel size for sampling
    vec2 pixelSize = 1.0 / screenSize;
    
    // Read current depth
    float centerDepth = texture(depthMap, TexCoords).r;
    
    // Skip smoothing for background pixels
    if (centerDepth >= 0.99) {
        FragColor = vec4(centerDepth);
        return;
    }
    
    // Get center normal (stored in [0,1] range, convert back to [-1,1])
    vec3 centerNormal = texture(normalMap, TexCoords).rgb * 2.0 - 1.0;
    
    // Apply curvature flow smoothing
    float smoothedDepth = centerDepth;
    
    // Apply multiple iterations for better quality
    for (int i = 0; i < smoothingIterations; i++) {
        float sumDepth = 0.0;
        float totalWeight = 0.0;
        
        // Sample 3x3 neighborhood
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                // Skip center pixel
                if (x == 0 && y == 0) continue;
                
                vec2 offset = vec2(float(x), float(y)) * pixelSize;
                vec2 sampleCoord = TexCoords + offset;
                
                float sampleDepth = texture(depthMap, sampleCoord).r;
                
                // Skip background pixels
                if (sampleDepth >= 0.99) continue;
                
                vec3 sampleNormal = texture(normalMap, sampleCoord).rgb * 2.0 - 1.0;
                
                // Calculate weights based on:
                // 1. Normal similarity (preserve edges)
                float normalWeight = max(0.0, dot(centerNormal, sampleNormal));
                normalWeight = pow(normalWeight, 2.0); // Emphasize normal similarity
                
                // 2. Depth similarity (prevent smoothing across depth discontinuities)
                float depthDiff = abs(centerDepth - sampleDepth);
                float depthWeight = exp(-depthDiff * 100.0);
                
                // 3. Spatial weight (closer pixels have more influence)
                float spatialWeight = 1.0 / (1.0 + length(vec2(x, y)));
                
                // Combined weight
                float weight = normalWeight * depthWeight * spatialWeight;
                
                sumDepth += sampleDepth * weight;
                totalWeight += weight;
            }
        }
        
        // Apply weighted average if enough valid neighbors
        if (totalWeight > 0.0) {
            float neighborAvg = sumDepth / totalWeight;
            // Mix with current depth based on smoothing strength
            smoothedDepth = mix(smoothedDepth, neighborAvg, smoothingStrength);
        }
    }
    
    // Output smoothed depth
    FragColor = vec4(smoothedDepth, smoothedDepth, smoothedDepth, 1.0);
}