vec3 DepthToWorld(vec2 screenPos, float depth) {
    vec4 clipSpacePos = vec4(screenPos*2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePos = scene.inverseProj*clipSpacePos;
    viewSpacePos /= viewSpacePos.w;
    vec4 worldSpacePos = scene.inverseView*viewSpacePos;
    return worldSpacePos.xyz;
}