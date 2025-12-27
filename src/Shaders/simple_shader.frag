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

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  ivec4 flags0; // useTexture, currentFrame, objectState, direction
  ivec4 flags1; // debugView, padding
} push;

void main() {
  vec3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
  vec3 specularLight = vec3(0.0);
  vec3 surfaceNormal = normalize(fragNormalWorld);

  // Debug normal view
  if (push.flags1.x == 1) {
    vec3 normalColor = surfaceNormal * 0.5 + 0.5;
    outColor = vec4(normalColor, 1.0);
    return;
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
    blinnTerm = pow(blinnTerm, 512.0); // higher values -> sharper highlight
    specularLight += intensity * blinnTerm;
  }

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

  vec3 color = fragColor;
  vec4 sampledColor = texture(diffuseMap, animatedUv);
  if (push.flags0.x == 1) {
    color = sampledColor.xyz;
  }
  
  outColor = vec4(diffuseLight * color + specularLight * fragColor, 1.0);

  if (sampledColor.a < 0.1) {
    discard;
  }
}
