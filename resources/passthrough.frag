#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D inputTexture;

void main()
{
    // Simply pass the texture color through
    FragColor = texture(inputTexture, TexCoords);
}