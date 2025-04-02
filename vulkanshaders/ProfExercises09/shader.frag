#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 interPos;
layout(location = 1) in vec4 interColor;
layout(location = 2) in vec3 interNormal;


void main() {
    vec3 N = normalize(interNormal);
    //outColor = vec4(1.0, 0.0, 0.0, 1.0);
    //outColor = interColor;
    N = (N + 1.0)/2.0;
    outColor = vec4(N, 1.0);
} 
