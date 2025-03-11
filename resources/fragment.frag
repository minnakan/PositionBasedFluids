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
    
    // Use a lighter version of the particle color
    vec3 lighterColor = mix(Color, vec3(1.0), 0.25); // Blend with white to make it lighter
    FragColor = vec4(lighterColor, 1.0);
}