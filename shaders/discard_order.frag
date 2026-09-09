#version 330 core

in vec2 screenUV;
uniform sampler2D sceneTexture;
uniform int iterationCount;
uniform int workBeforeDiscard;
out vec4 FragColor;

// This function simulates a workload by performing a series of arithmetic operations on the input color value.
// What do we mean by workload? In the context of shaders, a workload refers to the amount of computation
// that a shader performs for each fragment (pixel) it processes.
// This can include mathematical operations, texture lookups, and other calculations that affect the final color output.
// The more complex the workload, the more time it takes to process each fragment, which can impact performance and rendering speed.
vec3 repeatedWork(vec3 value)
{
    for (int i = 0; i < iterationCount; ++i)
    {
        float phase = float(i + 1) * 0.37;
        value += 0.01 * vec3(sin(value.r + phase), cos(value.g + phase),
                             sin(value.b - phase));
        value = clamp(value, 0.0, 1.0);
    }
    return value;
}

void main()
{
    vec3 value = texture(sceneTexture, screenUV).rgb;
    // Simplified work-order demonstration, not a complete overdraw model.
    if (workBeforeDiscard != 0)
        value = repeatedWork(value); // Perform workload before discard if specified.
    if (screenUV.x < 0.5) // Discard fragments on the left half of the screen to demonstrate discard behavior.
        discard;
    if (workBeforeDiscard == 0) // Perform workload after discard if specified.
        value = repeatedWork(value);
    FragColor = vec4(value, 1.0);
}
