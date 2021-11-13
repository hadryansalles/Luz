#version 450

#extension GL_GOOGLE_include_directive : enable

#include "base.glsl"

layout(location = 0) in vec4 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

vec3 viewDir;
vec4 diffuseColor;
vec4 specularColor;
float shineness;

vec4 EvalPointLight(LightBlock light) {
    vec3 lightVec = light.position - vec3(fragPos);
    float dist2 = dot(lightVec, lightVec);
    lightVec = normalize(lightVec);
    float diffuse = clamp(dot(lightVec, fragNormal), 0.0, 1.0)/dist2;
    
    vec3 reflectDir = reflect(-lightVec, fragNormal);
    float specular = pow(max(dot(viewDir, reflectDir), 0.0), shineness);
    
    return (diffuse*diffuseColor + specular*specularColor)*vec4(light.color*light.intensity, 1.0);
}

vec4 EvalDirectionalLight(LightBlock dirLight) {
    float diffuse = clamp(dot(-dirLight.direction, fragNormal), 0.0, 1.0);
    return vec4(0, 0, 0, 1);
    // return dirLight.color.xyz*clamp(dot(-dirLight.direction, fragNormal), 0.0, 1.0)*dirLight.intensity;
}

vec4 EvalSpotLight(LightBlock light) {
    vec3 lightVec = light.position - vec3(fragPos);
    float dist2 = dot(lightVec, lightVec);
    lightVec = normalize(lightVec);
    float theta = dot(lightVec, normalize(-light.direction));
    float epsilon = light.innerAngle - light.outerAngle;
    float diff = clamp(dot(fragNormal, -light.direction), 0.0, 1.0);
    return vec4(0, 0, 0, 1);
    // return light.color*diff*clamp((theta-light.outerAngle)/epsilon, 0.0, 1.0)*light.intensity/dist2;
}

void main() {
    // outColor = vec4(1, 1, 1, 1);
    shineness = model.values[0];
    diffuseColor = texture(textures[model.textures[0]], fragTexCoord)*model.colors[0];
    specularColor = texture(textures[model.textures[1]], fragTexCoord)*model.colors[1];
    viewDir = normalize(scene.camPos - fragPos.xyz);

    vec4 intensity = vec4(scene.ambientLightColor*scene.ambientLightIntensity, 1.0);
    for(int i = 0; i < scene.numLights; i++) {
        if(scene.lights[i].type == LIGHT_TYPE_POINT) {
            intensity += EvalPointLight(scene.lights[i]);
        } else if(scene.lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
            intensity += EvalDirectionalLight(scene.lights[i]);
        } else if(scene.lights[i].type == LIGHT_TYPE_SPOT) {
            intensity += EvalSpotLight(scene.lights[i]);
        }
    }
    outColor = intensity*diffuseColor;
}