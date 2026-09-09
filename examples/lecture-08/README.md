# Optional Lecture 8 code demonstrations

These tiny demonstrations support the Lecture 8 discussion of rendering
workload and optimization. They are optional live teaching aids, not extra
assignment requirements. Do not add them to your project unless they help
answer your project's own optimization question.

## Demo 1 — Fragment workload

1. Start in normal mode (`1`), then switch to workload mode (`2`).
2. Keep the window size, scene, camera and build configuration fixed.
3. Press the Down/Up arrow keys to halve/double the repeated-work count (up to 1024).
4. Record rough frame-time/FPS observations and compare the image quality.

Press `V` to toggle VSync. VSync is ON by default, preserving the normal
starter behavior; it can flatten observations around the display refresh
interval. Use VSync OFF when trying to see workload trends.

The iteration count is shared by workload and discard modes, so switching
between them keeps the same repeated-work setting for a fairer comparison.

The workload shader samples the existing scene texture, then performs repeated
deterministic sin/cos arithmetic controlled by `iterationCount`. Its output is
clamped so more iterations do not simply make the image brighter. Ask:

- What stayed fixed, and what changed?
- Why should the fragment cost trend upward?
- Does visible quality keep improving at the same rate?
- Is whole-frame timing enough to prove GPU shader cost precisely?

Whole-frame FPS/frame time also includes CPU work, waiting and other passes; it
is not direct GPU pass timing. This is an approximate workload-trend
experiment, not a rigorous benchmark. Keep the window size, camera and scene
fixed while changing one workload variable. VSync or another frame cap can
hide differences.

## Demo 2 — `discard` ordering

Switch to discard mode with `3`. Press `D` to compare repeated work before the
discard test with repeated work after it. The left half is discarded in both
variants, keeping the visible result comparable.

- Which path avoids more shader work?
- Why does `discard` not undo work already performed?
- Why is this only a simplified example of overdraw/transparency cost?

The shader deliberately isolates ordering inside one fragment invocation. It
does not model all rasterization, blending or overdraw behavior.

## Connection to your project

Use the same controlled experiment for a project-relevant variable, such as:
FBM octave count, ray-march sample count, screen-space sample count, wave count,
grid density or render-target resolution. Change one variable while keeping the
other conditions fixed, measure what you can, and report visual/technical trade-offs.

Controls are printed when modes or counts change. Return to normal mode with
`1` before leaving the starter.
