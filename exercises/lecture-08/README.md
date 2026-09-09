# Lecture 8 — Rendering Concepts & Graphics Optimization

> **Repository checkpoint:** This guide is written for `lecture-08`. Once
> published, the tag simply records the course/repository checkpoint and this
> exercise guide. Lecture 8 intentionally adds **no new rendering feature** to
> the common starter. Use your current project as the main material. If `main`
> has moved ahead later, inspect the published checkpoint with:
>
> ```powershell
> git fetch --tags
> git switch -c catchup-lecture-08 lecture-08
> ```
>
> You do not need to switch if your current project builds and you are working
> in your own project repository.

Thursday 10 September 2026, **09:00–15:30** — Futuregames GP25,
3D and Shader Programming. **09:00–10:00** is for project work, setup and
individual questions; important common material begins at **10:00**.

## Purpose

This is the last full teacher-led day before Friday's final workshop and
individual project deadline. The final shared reset covers two course requirements:
knowledge of wider rendering concepts, and applying/explaining one relevant
basic graphics optimization.

**Week 3 project completion has priority. This is a guide / evidence worksheet,
not a large exercise set to finish before project work.** Use the sections to
fill gaps in your existing project evidence, reusing what you already have.
You are not required to implement PBR, HDR or volumetrics, or to add an
optimization system that your project does not need.

**Predict -> change one thing -> run -> observe/measure -> compare -> explain**

## A. Wider rendering concept — explain one accurately

Choose **one** concept connected to your project or a relevant modern-renderer
extension. Understanding and accurate naming matter more than adding features.

### PBR — physically based rendering

PBR uses physically motivated material/light response to make materials behave
more consistently under different lighting and viewing conditions. Reconnect
to the surface normal **N**, light direction **L**, view direction **V**, and
material parameters such as roughness/metalness:

```text
N, L, V + material parameters -> reflectance model -> lighting response
```

No Cook–Torrance implementation or derivation is required.

### HDR — high dynamic range rendering

HDR preserves a wider numerical light/color range before mapping it to the
display range. Light values can exceed the usual display-white value of 1:

```text
lighting/emission -> HDR target -> optional effects -> tone mapping -> display
```

An emissive-looking color suggests a glowing source. HDR preserves the wider
range; bloom spreads bright contributions into surrounding pixels as an effect.
These are distinct: bright color alone proves neither HDR nor bloom, and HDR
does not require bloom. The common starter's `GL_RGBA8` scene target and
pass-through screen shader do not form an HDR/tone-mapping pipeline.

### Volumetric rendering

Surface shading evaluates a surface. Volumetric rendering samples/integrates
through a participating region such as fog, cloud or smoke, where light is
absorbed/scattered along a distance:

```text
ray -> samples/integration through medium -> accumulated color/transmittance
```

Transmittance describes how much light passes through the medium. A noisy
transparent object can be a useful approximation without being true volumetric
integration.

**Write 3–5 sentences about ONE concept:** what problem it solves, its important
data/stages, and how it relates to your project. State whether it is
**implemented, approximated, or simply a relevant modern-renderer connection**.

## B. Name the graphics workload

Identify the work your project actually performs. Use the categories that apply:

| Work category | Units / examples |
|---|---|
| CPU / submission | draw calls, state changes, generation/upload |
| Vertex work | vertex count, deformation, per-vertex math |
| Raster / coverage | triangles, screen coverage |
| Fragment work | fragment invocations, overdraw, blending/discard |
| Samples / loops | texture samples, FBM octaves, ray steps/iterations |
| Render target / memory | resolution, passes, bandwidth |

**What unit of work does your project spend, and which one can you deliberately
change?** Write one sentence naming the work and a controllable variable.

Examples: terrain grid resolution, wave count, FBM octaves, ray-march samples,
render-target resolution, outline samples, instancing/draw calls, LOD or culling.
Choose by project relevance; this is not a list of systems to implement.

## C. Apply one relevant optimization

Reduce or avoid identified work while preserving the central effect. Explain
what work you reduce, why that should help, and what quality/correctness might
change. Apply **one** relevant optimization in your project.

| Possible change | Work reduced / possible trade-off |
|---|---|
| 96 ray steps -> 48 + early termination | fewer executed loop iterations; possible missed detail or integration error |
| 512x512 target -> 256x256 | fewer target texels/fragments; lower detail, possible aliasing |
| Repeated draws -> instancing | lower CPU submission overhead; instance-data setup |
| High detail everywhere -> LOD | less distant geometry work; possible popping/detail loss |
| Cull work that cannot contribute | avoided draws/shading; visibility checks must remain correct |

For a controlled test, change ray-step count and early termination separately;
hold one fixed while testing the other. Lower target resolution does not
automatically reduce the number of samples taken per fragment.

**`discard` and transparency are not automatically free.** Raster work and
shader operations before `discard` can already have happened. Transparency
can add overdraw and blending work even when the visible contribution is small.
Identify what is actually avoided.

## D. Make a controlled comparison

Use this small experiment record directly in your project notes/report:

- **Hypothesis:** Changing ______ should reduce ______ because ______.
- **Independent workload variable:** ______ (one variable, with before/after values).
- **Controlled conditions:** camera, scene, seed/time, resolution unless it is
  the variable, hardware/build configuration, and other quality settings.
- **Measurement method:** tool/metric, units, measurement interval and repeats.
  Let startup/uploads settle; record VSync/frame caps, which can hide a change.

| Setting / workload value | Measurement (with units) | Visual/technical result |
|---|---|---|
| Before: ______ | ______ | ______ |
| After: ______ | ______ | ______ |

- **Trade-off:** What visible or technical quality changed? Compare matching views.
- **Conclusion/limitations:** Did the evidence support the hypothesis? What
  could the method not establish?

Use available GPU timing/profiling if understood, frame time, or CPU timing
when submission is the target. Work counts support the explanation but do not
by themselves prove a timing improvement. No new profiler library or UI is needed.
**A small or inconclusive result is acceptable if honestly reported.**

FPS is nonlinear; frame time in milliseconds is `1000 / FPS`:

```text
60 FPS  ≈ 16.7 ms/frame
120 FPS ≈  8.3 ms/frame
200 FPS ≈  5.0 ms/frame
```

Whole-frame FPS does not directly equal shader/GPU cost: CPU work, waiting and
other passes also affect it. An FPS percentage change is not the same percentage
change in shader cost. The common starter enables VSync with `glfwSwapInterval(1)`.

## E. Code-review self-check — four shared themes

These are anonymous shared lessons from project code reviews, not comparisons
or rankings. Check your central effect; use individual support for deeper
project-specific issues.

### 1. Trace the active path to the image

A shader, buffer or helper can exist without affecting the active result. Trace:

```text
resource/input -> active program/pass -> consuming operation
              -> draw/dispatch -> output/composition
```

**Which exact operation consumes this value now?** Prove the active path with
one unmistakable temporary change, such as a constant output color, then restore.

### 2. Coordinate spaces are part of the algorithm

Label local, world, view, clip, NDC, screen UV and texture UV(W) explicitly.
Variable names are claims, not proof: inspect the transformations. Values
compared together must use compatible spaces. If a surface is displaced,
ask whether its normals still describe the surface actually drawn.

### 3. Name what you actually implemented

Approximations are valid; inaccurate claims are not useful evidence:

- density != automatically opacity;
- camera distance != automatically water depth;
- bright color != automatically HDR/bloom;
- noisy soft transparent shape != automatically volumetric integration;
- copied normal math may stop being correct if the displacement equation changes.

### 4. One clean experiment beats an optimization story

Freeze what can be frozen, change one workload variable, measure, and discuss
the visual trade-off honestly.

## F. Turn one explanation into visible evidence

Isolate **one** important intermediate value/stage for a screenshot or demo:

- normal RGB;
- grayscale Fresnel term or `N dot V` (label which; they are not identical);
- dissolve/noise field;
- UV/screen UV;
- deformation/height texture;
- raw/linear depth (label which);
- one wave / one FBM octave;
- scene pass vs post-process pass.

**Predict -> visualize -> compare -> restore.** Remap values for display when
needed and explain the mapping. Reuse this evidence in your report/video.
This guide supports project completion; it is not an extra exercise sequence
to finish first.

## G. Submission rehearsal

Do a mock hand-in today while there is still time to fix something:

- [ ] **RUN:** Test build/run instructions, ideally from a clean/fresh state;
  confirm shaders, assets, dependencies and paths are available.
- [ ] **SHOW:** Make the central effect clearly visible and easy to reach.
- [ ] **EXPLAIN:** Trace shader inputs -> math/operations -> outputs, including
  important coordinate spaces.
- [ ] **PIPELINE:** Make the diagram match the actual project, not a generic
  copied diagram.
- [ ] **CONCEPT:** Relate one wider rendering concept accurately.
- [ ] **OPTIMIZATION:** Apply one relevant optimization; identify the workload
  and record evidence/trade-off.
- [ ] **REPORT:** Match the current implementation and remove outdated claims.
- [ ] **CREDITS:** Acknowledge external code, assets, tutorials and AI use.
- [ ] **VIDEO:** Plan roughly 1–3 minutes that make the result and comparisons
  easy to understand.

## H. Final scope decision

| Decision | Remaining work |
|---|---|
| **Keep** | Submission-critical correctness and evidence |
| **Postpone** | Polish/optional extensions beyond a sound submission |
| **Cut** | Work that threatens stability, explanation or completion |

For **VG, secure G evidence first**. VG depth should come from meaningful
advanced shader/mathematical work in the same central effect, not stacking
unrelated features.

## Finish

By leaving Thursday, aim for:

- a stable build;
- one wider concept explained;
- one applied optimization with preliminary evidence;
- a project-specific pipeline diagram;
- shader/math reasoning identified;
- a report aligned with the current implementation;
- a demo-video plan;
- only submission-critical Friday tasks remaining.

Project completion remains the priority. Friday's final workshop is for
troubleshooting, verification and course review.
The regular individual project deadline is **Friday 11 September 2026, 23:59**.
