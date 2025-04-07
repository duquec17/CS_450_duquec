#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 interPos;
layout(location = 1) in vec4 interColor;
layout(location = 2) in vec3 interNormal;

struct PointLight {
    vec4 pos;
    vec4 vpos;
    vec4 color;
};

layout(set=0, binding=1) uniform UBOFragment {
    PointLight light;
} ubo;

void main() {
    vec3 N = normalize(interNormal);
    //outColor = vec4(1.0, 0.0, 0.0, 1.0);
    //outColor = interColor;
    //N = (N + 1.0)/2.0;
    //outColor = vec4(N, 1.0);

    vec3 L = vec3(ubo.light.vpos) - interPos;
    float d = length(L);
    L = normalize(L);
    float ref = 1.0;
    float att = ref/(d*d + ref);
    vec4 colorAtt = vec4(att,att,att,1.0);

    float diffuse = max(0, dot(L, N));
    vec3 diffColor = diffuse*vec3(ubo.light.color)*vec3(interColor);

    outColor = vec4(diffColor, 1.0);
} 
