const float pi = 3.1415927;
const float bottomRadius = 6360e3;
const float topRadius = 6460e3;
const float rayleighScaleHeight = 8e3;
const float mieScaleHeight = 1.2e3;
const vec3 rayleighScatteringCoefficient = vec3(5.8e-6, 13.5e-6, 33.1e-6);
const float mieScatteringCoefficient = 3.996e-06;
const float mieExtinctionCoefficient = 4.440e-06;
const vec3 ozoneAbsorptionCoefficient = vec3(0.650e-6, 1.881e-6, 0.085e-6);

float RayleighPhase(float angle) {
    return 3.0 / (16.0 * pi) * (1.0 + (angle * angle));
}

float MiePhase(float angle) {
    float g = 0.8;
    return 3.0 / (8.0 * pi) * ((1.0 - g * g) * (1.0 + angle * angle)) / ((2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * angle, 1.5));
}

mat3 RotationAxisAngle(vec3 axis, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    vec3 column1 = axis * axis.x * (1.0 - c) + vec3(c, axis.z * s, -axis.y * s);
    vec3 column2 = axis * axis.y * (1.0 - c) + vec3(-axis.z * s, c, axis.x * s);
    vec3 column3 = axis * axis.z * (1.0 - c) + vec3(axis.y * s, -axis.x * s, c);
    return mat3(column1, column2, column3);
}

bool IntersectSphere(vec3 rayOrigin, vec3 rayDirection, float radius, inout float t1, inout float t2) {
    float b = dot(rayDirection, rayOrigin);
    float c = dot(rayOrigin, rayOrigin) - radius * radius;
    float d = b * b - c;
    if (d < 0.0) {
        return false;
    }
    t1 = -b - sqrt(d);
    t2 = -b + sqrt(d);
    return true;
}

bool IntersectSphere(vec3 rayOrigin, vec3 rayDirection, float radius, inout float t) {
    float b = dot(rayDirection, rayOrigin);
    float c = dot(rayOrigin, rayOrigin) - radius * radius;
    float d = b * b - c;
    if (d < 0.0) {
        return false;
    }
    t = -b - sqrt(d);
    return true;
}

vec3 ACES2(vec3 color) {
    return clamp((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
}