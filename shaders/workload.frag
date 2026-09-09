#version 330 core

in vec2 screenUV;
uniform sampler2D sceneTexture;
uniform int iterationCount;
out vec4 FragColor;

void main()
{
    vec3 base = texture(sceneTexture, screenUV).rgb;
    vec3 value = base;
    // Deliberate, deterministic fragment work. Normalization keeps the image
    // comparable while iterationCount changes the amount of arithmetic.
    for (int i = 0; i < iterationCount; ++i)
    {
        float phase = float(i + 1) * 0.37; // arbitrary phase offset to avoid simple repetition.

        // Below: A simple, deterministic, and cheap-to-compute function that produces a small perturbation.
        // Why? Because we want to simulate a workload that is not just a simple copy of the input color,
        // but rather some computation that could represent a more complex shader operation.
        // This helps in testing performance and behavior under different workloads.
        value += 0.01 * vec3(sin(value.r + phase), cos(value.g + phase),
                             sin(value.b - phase));

        // Clamp the value to ensure it stays within the [0, 1] range for valid color output.
        value = clamp(value, 0.0, 1.0);
    }
    FragColor = vec4(value, 1.0);
}
