#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;

out vec4 FragColor;

uniform vec3 uColor;
uniform vec3 uLightPos;
uniform vec3 uViewPos;

void main(){
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);

    float diff = max(dot(N,L), 0.0);

    vec3 viewDir = normalize(uViewPos - vWorldPos);
    vec3 reflectDir = reflect(-L, N);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

    vec3 ambient = 0.08 * uColor;
    vec3 color = ambient + (0.9 * diff + 0.4 * spec) * uColor;

    FragColor = vec4(color, 1.0);
}
