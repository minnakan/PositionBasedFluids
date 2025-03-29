#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D inputTexture;

void main()
{
    // Simply pass the texture color through
    //FragColor = texture(inputTexture, TexCoords);
    vec4 depthValue = texture(inputTexture, TexCoords);
    FragColor = vec4(vec3(depthValue.r), 1.0);
}