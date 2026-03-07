#pragma stage vertex
#version 450

void main()
{
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}

// --------------------------------------------------------------------------------------------

#pragma stage tessControl
#version 450

layout(vertices = 3) out;

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    if (gl_InvocationID == 0)
    {
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = 1.0;
        gl_TessLevelOuter[2] = 1.0;
        gl_TessLevelInner[0] = 1.0;
    }
}

// --------------------------------------------------------------------------------------------

#pragma stage tessEvaluation
#version 450

layout(triangles, equal_spacing, ccw) in;

void main()
{
    gl_Position = gl_TessCoord.x * gl_in[0].gl_Position
    + gl_TessCoord.y * gl_in[1].gl_Position
    + gl_TessCoord.z * gl_in[2].gl_Position;
}

// --------------------------------------------------------------------------------------------

#pragma stage geometry
#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

void main()
{
    for (int i = 0; i < 3; ++i)
    {
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}

// --------------------------------------------------------------------------------------------

#pragma stage fragment
#version 450

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(1.0);
}