#version 430 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 waterColor = vec3(0.2, 0.4, 0.8);
uniform float ambient = 0.2;
uniform float specular = 0.7;
uniform float shininess = 64.0;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

    vec3 ambientColor = ambient * waterColor;
    vec3 diffuseColor = diff * waterColor;
    vec3 specularColor = specular * spec * vec3(1.0);

    vec3 finalColor = ambientColor + diffuseColor + specularColor;
    FragColor = vec4(finalColor, 0.85);
}
