#version 430 core
out vec4 FragColor;

in float Depth;

void main()
{
    // Calculate distance from center of point sprite
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);
    
    if (r2 > 1.0) {
        discard;
    }
    
    float z = sqrt(1.0 - r2);
    
    // Original depth from vertex shader
    float originalDepth = gl_FragCoord.z;
    
    float depth = originalDepth - z * 0.05;
    gl_FragDepth = depth;
    
    float depthVisualization;
    
    // Adjust these bounds based on your scene's typical depth range
    float nearDepth = 0.85; 
    float farDepth = 1.0;   
    
    /*
    depthVisualization = (farDepth - depth) / (farDepth - nearDepth);
    depthVisualization = clamp(depthVisualization, 0.0, 1.0);
    depthVisualization = pow(depthVisualization, 0.5);
    vec3 color = mix(vec3(0.0, 0.0, 0.8), vec3(0.8, 0.0, 0.0), depthVisualization);*/
    
    
    FragColor = vec4(depth * vec3(1.0,1.0,1.0), 1.0);
}