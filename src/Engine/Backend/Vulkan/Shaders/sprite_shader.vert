#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUv;

struct PointLight {
  vec4 position; // ignore w
  vec4 color;    // w is intensity
};

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  mat4 invView;
  vec4 ambientLightColor; // w is intensity
  PointLight pointLights[10];
  int numLights;
} ubo;

layout(set = 1, binding = 0) uniform GameObjectBufferData {
  mat4 modelMatrix;
  mat4 normalMatrix;
} gameObject;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  int useTexture;
  int currentFrame;
  int objectState;
  int direction;
  int debugView;
  int atlasCols;
  int atlasRows;
  int startFrame;
  int rowIndex;
  int uvTransformFlags;
  vec4 uvOffset;
} push;

void main() {
  vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
  gl_Position = ubo.projection * ubo.view * positionWorld;
  fragUv = uv;
  fragColor = color;
}
