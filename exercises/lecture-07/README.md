# Lecture 7 — Render Targets, Screen Space & Post-Processing

> **Repository checkpoint:** This exercise was written for the `lecture-07`
> baseline. Once published, that tag will identify the known-good Lecture 7
> starting state. If `main` has already moved ahead, create a catch-up branch:
>
> ```powershell
> git fetch --tags
> git switch -c catchup-lecture-07 lecture-07
> ```
>
> If your current code already matches the exercise and works, you do not need
> to switch.

## Purpose

The central question today is:

**How can we render a scene into a texture, then sample that image in a second pass?**

**Week 3 project completion has priority.** Learn the core two-pass path, make
one small experiment, and return to your project. These short investigations
support understanding and debugging; they are not a large worksheet to finish
before doing project work.

Lecture 6 was a project/debugging workshop and introduced no new common
baseline. This renderer builds directly from the Lecture 5 lit, textured cube.

Use the course workflow throughout:

**Predict -> change one thing -> run -> observe -> compare -> explain**

The main files are `src/main.cpp`, `shaders/screen.vert`, and
`shaders/screen.frag`. The existing `basic.vert` and `basic.frag` still render
the scene. Rebuild after shader edits so CMake copies them beside the executable.
Restore the pass-through screen shader between experiments.

## Recommended supporting reading (optional)

Use these alongside the code when a concept needs another explanation. They
are not prerequisites or extra assignments; Week 3 project work still comes first.

- **Main companion:** [LearnOpenGL — Framebuffers](https://learnopengl.com/Advanced-OpenGL/Framebuffers).
  Focus on framebuffer creation, texture versus renderbuffer attachments, and
  displaying the rendered texture on a fullscreen quad. Compare its two passes
  with ours. Inversion or grayscale is enough for a first effect; skip the
  kernel, blur, and edge-detection sections for this lecture.
- **Texture refresher:** [LearnOpenGL — Textures](https://learnopengl.com/Getting-started/Textures).
  Revisit texture coordinates, filtering, wrapping, and texture units if the
  sampler binding is unclear. Our scene texture is filled by rendering, so you
  do not need the image-loading setup from the tutorial.
- **Course recap:** [Lecture 5 — Texturing & Applied Surface Effects](../lecture-05/README.md).
  Revisit the distinction between a texture object, texture unit, and sampler.
  The same relationship now lets the screen shader read the scene image.

While reading, ask: **Where is this draw writing, and which texture is this
shader reading?** Find the matching bindings in `src/main.cpp` rather than
copying an entire tutorial program.

## A. Inspect the known-good two-pass renderer

Build and run. Expect the same static lit red/cyan checker cube on a dark
background, with approximately the same appearance as Lecture 5.

Trace this path in the source:

```text
scene geometry
    -> scene shader
    -> off-screen framebuffer
    -> scene color texture
    -> fullscreen quad
    -> screen shader
    -> default framebuffer
```

Find the two draw calls, framebuffer bindings, color attachment, depth/stencil
renderbuffer, and sampler binding. Identify what the CPU creates/configures
and what the GPU draws or samples.

- Which pass renders the cube? Which renders the fullscreen quad?
- Where is depth testing required, and why is it disabled for pass 2?
- Why can `screen.frag` sample the first pass? Why is its color attachment a
  texture while the depth/stencil attachment can be a renderbuffer?
- What does framebuffer `0` mean? Which framebuffer belongs to the window?
- Why does the visual result initially look almost unchanged?

Resize the window. Find where GLFW's framebuffer size controls attachment
storage, the projection aspect ratio, and both viewports. What happens when
the drawable width or height is zero?

## B. Visualize screenUV

Temporarily replace the output in `screen.frag` with:

```glsl
FragColor = vec4(screenUV, 0.0, 1.0);
```

Predict where red (U) and green (V) increase, then compare with the image.
How do coordinates across this fullscreen image differ in meaning from the
cube's own surface UVs? Why does this gradient cover the background too?

Restore `FragColor = texture(sceneTexture, screenUV);` afterward.

## C. First post-process

Choose **one** small operation. For invert, replace the screen output with:

```glsl
vec3 color = texture(sceneTexture, screenUV).rgb;
FragColor = vec4(vec3(1.0) - color, 1.0);
```

Alternatively, use grayscale:

```glsl
vec3 color = texture(sceneTexture, screenUV).rgb;
float gray = dot(color, vec3(0.2126, 0.7152, 0.0722));
FragColor = vec4(vec3(gray), 1.0);
```

Predict the effect on both cube and background. Why can the complete scene
now change without modifying the cube's scene shader? Restore pass-through.

## D. Offset sampling

Replace the screen output with:

```glsl
vec2 sampleUV = screenUV + vec2(0.02, 0.0);
FragColor = texture(sceneTexture, sampleUV);
```

Predict which direction the image appears to move. A positive read offset
reads farther right in the stored image; is that the same as moving the cube
right? Explain the difference between moving geometry/the camera and changing
where pass 2 reads an already-rendered image. What does clamp-to-edge do near
the image boundary? Restore direct sampling.

## E. One-texel screen-space reasoning

Try reading approximately one texel to the right:

```glsl
vec2 texelSize =
    1.0 / vec2(textureSize(sceneTexture, 0));
vec2 sampleUV = screenUV + vec2(texelSize.x, 0.0);
FragColor = texture(sceneTexture, sampleUV);
```

`screenUV` measures normalized texture-coordinate distance. `textureSize`
returns the texture dimensions at mip level 0; the reciprocal converts one
texture element into that normalized domain. For a 900-wide texture, one texel
is `1.0 / 900.0` in U, while `0.02` spans 18 texels.

A constant UV offset and a one-pixel/one-texel offset are different concepts.
Here the render target matches the window framebuffer, so one source texel
corresponds to one output framebuffer pixel. A logical window unit on a
high-DPI display need not be one framebuffer pixel. If the render target had
a different resolution, source texels and output pixels would differ too.

Compare the constant and one-texel offsets at two window sizes, changing one
thing at a time. Why does this distinction matter for resolution-dependent
screen-space effects? The one-texel movement may be subtle; inspect a cube
edge. Restore direct sampling afterward.

## F. Controlled debugging experiment

Start from the restored baseline. In pass 2 of `main.cpp`, temporarily change:

```cpp
glUniform1i(sceneTextureLocation, 0);
```

to:

```cpp
glUniform1i(sceneTextureLocation, 1);
```

Leave the scene color texture bound on `GL_TEXTURE0`. In this baseline nothing
binds a texture to unit 1, so the sampler no longer reads the scene image.
Predict the failure and run. Diagnose with evidence, one step at a time:

1. Output a constant from `screen.frag`, such as `vec4(1.0, 0.0, 1.0, 1.0)`.
   Does this show that the fullscreen draw reaches the output?
2. Visualize `screenUV`. Does the expected gradient confirm the coordinate path?
3. Restore texture sampling and inspect the sampler/texture-unit assumptions.
   Compare the integer uploaded to `sceneTexture` with `glActiveTexture` and
   `glBindTexture` in pass 2.
4. Restore sampler unit `0`, rebuild, and verify the cube returns.

Connect this to Lecture 5: a **texture object** owns image storage, a **texture
unit** holds a binding, and a **sampler uniform** stores a unit index, not a
texture object ID. Constant/UV output isolates parts of the path; it does not
prove the first pass or texture binding is correct. Keep framebuffer bindings
unchanged throughout this experiment.

## G. Project bridge

1. Could your project actually benefit from rendering into a texture or
   processing an already-rendered image?
2. If yes, what does the first pass produce?
3. What would the second pass sample/change?
4. If not, what does your actual project need more urgently?

**Do not add post-processing to your project merely because we learned it today.**

## Optional/stretch

Only if the core path is clear and your project is on track, try one small
vignette, RGB channel offset, simple pixelation, neighboring-texel inspection,
or reduced render-target resolution experiment. For reduced resolution, keep
the first-pass viewport and attachments consistent, and compare source texels
with output pixels. Restore the baseline afterward.

## Finish

Restore the pass-through screen shader, sampler unit 0, and full-size render
target. Verify the static lit checker cube, including after resize and
minimize/restore. Be ready to explain the two draws and how the first pass's
output becomes the second pass's input. Then return to Week 3 project work.
