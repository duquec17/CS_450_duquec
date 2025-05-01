#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec4 interPos;
layout(location = 2) in vec3 interNormal;

const float PI = 3.14159265359;

struct PointLight {
    vec4 pos;
    vec4 vpos;
    vec4 color;
};

layout(set = 0, binding = 1) uniform UBOFragment {
    PointLight light;
    float metallic;
    float roughness;
} ubo;

vec3 getFresnelAtAngleZero(vec3 albedo, float metallic) {
    // Default F0 for insulators (non-metallic)
    vec3 F0 = vec3(0.04);
    // Interpolate between default F0 and albedo based on metallic value
    F0 = mix(F0, albedo, metallic);
    return F0;
}

vec3 getFresnel(vec3 F0, vec3 L, vec3 H) {
    float cosAngle = max(dot(L, H), 0.0);
    return F0 + (1.0 - F0) * pow(1.0 - cosAngle, 5.0);
}

float getNDF(vec3 H, vec3 N, float roughness) {
    float alpha = roughness * roughness;
    float alphaSq = alpha *alpha;

    float NdotH = max(dot(N, H), 0.0);
    float NdotHSq = NdotH * NdotH;

    float denom = NdotHSq * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * denom * denom); 
}

float getSchlickGeo(vec3 B, vec3 N, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float NdotB = max(dot(N, B), 0.0);
    return NdotB / (NdotB * (1.0 - k) + k);
}

float getGF(vec3 L, vec3 V, vec3 N, float roughness) {
    float GL = getSchlickGeo(L, N, roughness);
    float GV = getSchlickGeo(V, N, roughness);
    return GL * GV;
}

layout(location = 0) out vec4 outColor;

void main() {
    // Normalize surface
    vec3 N = normalize(interNormal);

    // Calculate light vector
    vec3 L = normalize(vec3(ubo.light.vpos) - vec3(interPos));

    // Base color
    vec3 baseColor = vec3(fragColor);

    // Calculate the normalized view vector
    vec3 V = normalize(-vec3(interPos));

    // Calculate F0
    vec3 F0 = getFresnelAtAngleZero(baseColor, ubo.metallic);

    // Calculate the normalized half-vector
    vec3 H = normalize(L + V);

    // Calculate Fresnel reflectance F
    vec3 F = getFresnel(F0, L, H);

    // Set specular color
    vec3 kS = F;

    // Diffuse color
    vec3 kD = (1.0 - kS) * (1.0 - ubo.metallic) * baseColor / PI;

    // Calculate NDF
    float NDF = getNDF(H, N, ubo.roughness);

    // Calculate geometry function
    float G = getGF(L, V, N, ubo.roughness);

    // Complete specular reflection
    vec3 specular = (kS * NDF * G) / (4.0 * max(dot(N, L), 0.0) * max(dot(N, V), 0.0) + 0.0001);

    // Gamma-correct (linear to sRGB)
    vec3 finalColor = (kD + specular) * vec3(ubo.light.color) * max(dot(N, L), 0.0);

    //finalColor.rgb = pow(finalColor.rgb, vec3(2.2));

    // Output final color
    outColor = vec4(finalColor,1.0);

    //float diffComp = max(dot(N,L), 0);
    //outColor = vec4(diffComp, diffComp, diffComp, 1.0);
    //outColor = vec4(vec3(ubo.light.color), 1.0);

} 
