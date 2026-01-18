#version 450

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec3 fragNormalWorld;
layout (location = 3) in vec2 fragUv;

layout (location = 0) out vec4 outColor;

struct PointLight {
  vec4 position; // ignore w
  vec4 color;    // w is intensity
};

layout (set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  mat4 invView;
  vec4 ambientLightColor; // w is intensity
  PointLight pointLights[10];
  int numLights;
} ubo;

layout (set = 1, binding = 1) uniform sampler2D diffuseMap;
layout (set = 1, binding = 2) uniform sampler2D normalMap;
layout (set = 1, binding = 3) uniform sampler2D metallicRoughnessMap;
layout (set = 1, binding = 4) uniform sampler2D occlusionMap;
layout (set = 1, binding = 5) uniform sampler2D emissiveMap;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  ivec4 flags0; // textureMask, currentFrame, objectState, direction
  vec4 baseColor;
  vec4 emissiveMetallic; // emissive.rgb, metallic.a
  vec4 misc; // roughness, occlusionStrength, normalScale, debugView
} push;

mat3 cotangentFrame(vec3 normal, vec3 position, vec2 uv) {
  vec3 dp1 = dFdx(position);
  vec3 dp2 = dFdy(position);
  vec2 duv1 = dFdx(uv);
  vec2 duv2 = dFdy(uv);

  vec3 dp2perp = cross(dp2, normal);
  vec3 dp1perp = cross(normal, dp1);
  vec3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
  float invMax = inversesqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
  return mat3(tangent * invMax, bitangent * invMax, normal);
}

void main() {
  vec3 ambientLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
  vec3 diffuseLight = vec3(0.0);
  vec3 specularLight = vec3(0.0);
  vec3 surfaceNormal = normalize(fragNormalWorld);

  // sprite texture animation
  vec2 uvOffset = vec2(0.0);
  float frameWidth = 1.0 / 6.0; // default columns
  if (push.flags0.z == 1) {
    frameWidth = 1.0;
  }
  float frameHeight = 1.0 / 2.0; // rows
  uvOffset.x = frameWidth * float(push.flags0.y);
  uvOffset.y = frameHeight * float(push.flags0.z);
  vec2 animatedUv = fragUv + uvOffset;
  if (push.flags0.w == 2) { // LEFT
    animatedUv.x = 1.0 - animatedUv.x;
  }

  bool hasBaseColor = (push.flags0.x & 1) != 0;
  bool hasNormal = (push.flags0.x & (1 << 1)) != 0;
  bool hasMetallicRoughness = (push.flags0.x & (1 << 2)) != 0;
  bool hasOcclusion = (push.flags0.x & (1 << 3)) != 0;
  bool hasEmissive = (push.flags0.x & (1 << 4)) != 0;

  if (hasNormal) {
    vec3 tangentNormal = texture(normalMap, animatedUv).xyz * 2.0 - 1.0;
    tangentNormal.xy *= push.misc.z;
    mat3 tbn = cotangentFrame(surfaceNormal, fragPosWorld, animatedUv);
    surfaceNormal = normalize(tbn * tangentNormal);
  }

  // Debug normal view
  if (push.misc.w > 0.5) {
    vec3 normalColor = surfaceNormal * 0.5 + 0.5;
    outColor = vec4(normalColor, 1.0);
    return;
  }

  float metallic = clamp(push.emissiveMetallic.a, 0.0, 1.0);
  float roughness = clamp(push.misc.x, 0.0, 1.0);
  float occlusion = clamp(push.misc.y, 0.0, 1.0);
  vec3 emissive = push.emissiveMetallic.rgb;

  vec3 albedo = fragColor;
  if (hasBaseColor) {
    vec4 sampledColor = texture(diffuseMap, animatedUv);
    if (sampledColor.a < 0.1) {
      discard;
    }
    albedo = sampledColor.rgb;
  }
  albedo *= push.baseColor.rgb;

  if (hasMetallicRoughness) {
    vec4 mrSample = texture(metallicRoughnessMap, animatedUv);
    metallic = clamp(metallic * mrSample.b, 0.0, 1.0);
    roughness = clamp(roughness * mrSample.g, 0.0, 1.0);
  }
  if (hasOcclusion) {
    float occSample = texture(occlusionMap, animatedUv).r;
    occlusion = clamp(occlusion * occSample, 0.0, 1.0);
  }
  if (hasEmissive) {
    emissive *= texture(emissiveMap, animatedUv).rgb;
  }

  vec3 cameraPosWorld = ubo.invView[3].xyz;
  vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld);

  for (int i = 0; i < ubo.numLights; i++) {
    PointLight light = ubo.pointLights[i];
    vec3 directionToLight = light.position.xyz - fragPosWorld;
    float attenuation = 1.0 / dot(directionToLight, directionToLight); // distance squared
    directionToLight = normalize(directionToLight);

    float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
    vec3 intensity = light.color.xyz * light.color.w * attenuation;

    diffuseLight += intensity * cosAngIncidence;

    // specular lighting
    vec3 halfAngle = normalize(directionToLight + viewDirection);
    float blinnTerm = dot(surfaceNormal, halfAngle);
    blinnTerm = clamp(blinnTerm, 0, 1);
    float specPower = mix(8.0, 512.0, 1.0 - roughness);
    blinnTerm = pow(blinnTerm, specPower); // higher values -> sharper highlight
    specularLight += intensity * blinnTerm;
  }

  vec3 diffuseColor = albedo * (1.0 - metallic);
  vec3 specularColor = mix(vec3(0.04), albedo, metallic);
  vec3 litColor = (ambientLight + diffuseLight) * diffuseColor + specularLight * specularColor;
  litColor *= occlusion;
  litColor += emissive;

  outColor = vec4(litColor, 1.0);
}
