#version 330 core

// Matches the separate fullscreen-quad VAO: position.xy followed by uv.xy.
layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec2 aUV;

// Rasterization interpolates these coordinates across the fullscreen triangles.
out vec2 screenUV;

void main()
{
    screenUV = aUV;
    // Already in clip space. With w = 1, perspective division leaves XY unchanged.
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
