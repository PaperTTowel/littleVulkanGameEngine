# Engine Coordinate System

This file captures the agreed coordinate conventions so render and physics stay aligned.

## World axes
- +X: right
- +Y: up
- +Z: forward (objects/camera look toward +Z) → effectively left-handed (Z-forward).
- Units: 1.0 = 1 meter (use the same in physics).

## Camera
- View: `setViewYXZ` uses yaw/pitch around +Z forward.
- Projection: Vulkan-style perspective (NDC depth 0..1).
- Default viewer: starts at z = -2.5 looking toward +Z.

## Textures/UV
- Images are loaded with `stbi_set_flip_vertically_on_load(1)` so UV (0,0) is bottom-left in shader space.

## Sprites
- Rendered with alpha blending, depth off; facing +Z (billboard/rotate as needed).

## Physics interop (when integrating)
- Many physics engines use +Y up, but forward = -Z (right-handed). To convert between our world (+Z forward) and a RH -Z world:
  - Render → Physics: (x, y, -z), quaternion (x, y, -z, w).
  - Physics → Render: (x, y, -z), quaternion (x, y, -z, w).
- Keep transforms in meters; apply all axis flips in a single bridge helper to avoid drift.

