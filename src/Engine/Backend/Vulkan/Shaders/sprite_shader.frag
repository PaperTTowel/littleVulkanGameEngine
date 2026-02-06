#version 450

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec2 fragUv;

layout (location = 0) out vec4 outColor;

layout (set = 1, binding = 1) uniform sampler2D diffuseMap;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  int useTexture;   // 0 = vertex color, 1 = texture sample, 2 = texture with flip
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
  // simple atlas animation: columns x rows = 6 x 2 by default
  float cols = max(1.0, float(push.atlasCols));
  float rows = max(1.0, float(push.atlasRows));
  float frameWidth = 1.0 / cols;
  float frameHeight = 1.0 / rows;
  float frameIndex = float(push.startFrame + push.currentFrame);
  vec2 frameOffset = vec2(frameWidth * frameIndex, frameHeight * float(push.rowIndex));

  vec2 localUv = fragUv;
  if (push.direction == 2) { // LEFT
    localUv.x = 1.0 - localUv.x;
  }

  bool flipH = (push.uvTransformFlags & 1) != 0;
  bool flipV = (push.uvTransformFlags & 2) != 0;
  bool flipD = (push.uvTransformFlags & 4) != 0;
  if (flipD) {
    // Tiled diagonal flag is anti-diagonal flip. This branch follows Tiled's
    // H/V/D combination rules for orthogonal maps.
    localUv = localUv.yx;
    if (!flipH) {
      localUv.x = 1.0 - localUv.x;
    }
    if (!flipV) {
      localUv.y = 1.0 - localUv.y;
    }
  } else {
    if (flipH) {
      localUv.x = 1.0 - localUv.x;
    }
    if (flipV) {
      localUv.y = 1.0 - localUv.y;
    }
  }
  vec2 animatedUv = frameOffset + localUv * vec2(frameWidth, frameHeight) + push.uvOffset.xy;

  vec4 sampledColor = texture(diffuseMap, animatedUv);

  // basic unlit sprite
  vec3 baseColor = (push.useTexture == 1) ? sampledColor.xyz : fragColor;
  outColor = vec4(baseColor, sampledColor.a);

  if (sampledColor.a < 0.1) {
    discard;
  }
}
