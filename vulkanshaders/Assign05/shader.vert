#version 450

layout(push_constant) uniform PushConstants {
    mat4 modelMat;
    mat4 normMat;
} pc;

layout(std140, binding = 0) uniform matrices {
    mat4 viewMat;
    mat4 projMat;
}ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 interPos;
layout(location = 2) out vec3 interNormal;

void main() {
    gl_Position = ubo.projMat * ubo.viewMat * pc.modelMat * vec4(inPosition, 1.0);
    fragColor = inColor;
    interPos = ubo.viewMat * pc.modelMat * vec4(inPosition, 1.0);
    interNormal = mat3(pc.normMat)*inNormal;
} 
