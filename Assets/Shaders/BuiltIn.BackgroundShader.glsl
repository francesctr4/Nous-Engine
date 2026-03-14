#pragma stage vertex
#version 450

layout(location = 0) out vec2 outUV;

void main()
{
    // Fullscreen triangle via vertex index — no vertex buffer required.
    // Indices 0,1,2 produce a triangle that covers the entire NDC cube:
    //   0 -> uv=(0,0), ndc=(-1,-1)
    //   1 -> uv=(2,0), ndc=( 3,-1)
    //   2 -> uv=(0,2), ndc=(-1, 3)
    // With the inverted Vulkan viewport (y=H, height=-H):
    //   NDC.y=-1 maps to screen bottom  -> outUV.y = 0
    //   NDC.y=+1 maps to screen top     -> outUV.y = 1 (by interpolation)
    //   UV.y = (NDC.y + 1) / 2
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outUV = uv;
    gl_Position = vec4(uv * 2.0f - 1.0f, 0.9999f, 1.0f);
}

// ------------------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    vec4  skyColor;     // deep blue — used at top and bottom edges
    vec4  horizonColor; // near-white — used at the world y=0 horizon line
    float horizonY;     // UV-y of the world y=0 plane (0=bottom, 1=top), computed on CPU
} push;

void main()
{
    // Distance from the horizon line in UV space.
    // horizonY is the screen-space UV-y where world y=0 projects,
    // so the gradient is always anchored to the real 3D horizon.
    float dist = abs(inUV.y - push.horizonY);

    // Normalise: gradient always reaches full skyColor at the farthest screen edge.
    float maxDist = max(push.horizonY, 1.0f - push.horizonY);
    float t = clamp(dist / max(maxDist, 0.001f), 0.0f, 1.0f);

    // Sqrt ease: fast departure from white at the horizon, slower near the edges.
    t = sqrt(t);

    outColor = mix(push.horizonColor, push.skyColor, t);
}
