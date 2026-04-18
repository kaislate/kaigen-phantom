# Kaigen Phantom — Sound Design Guide

Tips for reducing artifacts and dialing in a clean, musical result.

---

## Reducing Pops and Crackles

Pops happen when the synthesizer makes abrupt amplitude changes. The most common triggers:

### Gate Threshold (RESYN mode)
The gate decides whether each individual wavelet (one cycle of the input waveform) is loud enough to synthesize. When the gate flaps rapidly — opening and closing on consecutive wavelets — you hear it as a stutter or crackle.

- **Best practice:** Set the gate just above your noise floor, not into the signal itself. Raise it slowly until background hiss disappears, then stop.
- **If it stutters:** Lower the gate. The threshold is cutting into real signal transients.
- **If it crackles on soft notes:** Lower the gate. Quiet notes are being partially gated.
- **If it passes too much noise:** Raise it slightly and compensate with less Phantom Strength.

### Boost Threshold + Boost Amount
Upward expansion (Threshold & Boost) applies extra gain to wavelets that exceed the threshold. The higher the Boost Amount, the larger the gain jump between wavelets. Even with the smoothing added, very high boost amounts on signals with uneven dynamics produce audible pumping.

- **Best practice:** Keep Boost Amount under 0.6 for most material. Use it as a subtle transient enhancer, not a compressor bypass.
- **If it pumps:** Lower Boost Amount first. Then raise Boost Threshold so fewer wavelets trigger it.
- **On sustained bass notes:** Boost works well. On plucked or percussive bass it can over-emphasize attack transients.

### Tracking Speed
Fast tracking means the plugin re-estimates the pitch aggressively at every zero crossing. On noisy or transient-heavy material this causes the estimated period to jump around, producing harsh harmonic "smearing."

- **Best practice:** Use the slowest tracking speed that still follows pitch changes cleanly. For bass guitar or synth bass, 0.15–0.25 is usually right. For faster melodic lines, 0.3–0.45.
- **If it sounds gritty on attacks:** Lower tracking speed. Let it glide into new notes rather than snapping.
- **If pitch changes lag too much:** Raise it incrementally.

### Phantom Threshold (Crossover Frequency)
This sets the cutoff below which the plugin processes the signal. Zero crossings at very low frequencies (under 40 Hz) are far apart and easily confused by noise — the synth can misfire.

- **Best practice:** Set the crossover just below your source's fundamental. For bass guitar (open E ≈ 41 Hz), 35–40 Hz is a safe floor. Don't set it lower than you need.
- **Dragging it too low** feeds subsonic content to the zero-crossing detector, where it doesn't belong.

### Wavelet Length
At full length (1.0), each synthesized cycle plays completely before the next begins. Shorter lengths truncate the cycle and create more silence between wavelets — which is useful for rhythmic effects but introduces more on/off transitions.

- **For clean, continuous sound:** Keep length at 0.85–1.0.
- **For gated, stutter effects:** Pull it below 0.5 deliberately.

---

## Getting a More Pleasant Sound

### Use H1 as Your Foundation
H1 (the fundamental reconstruction) is a clean sine wave at the detected pitch. It acts as glue — it stays smooth and musical regardless of how aggressive the H2–H8 recipe is.

- For warm, full-sounding results: keep H1 at 0.8–1.0.
- If the output sounds hollow or thin: raise H1 before reaching for H2–H8.

### Recipe Wheel — Start Sparse
H2 and H3 are the most musical upper harmonics for bass. H4–H8 add edge and grit.

- **Warm/round:** H2 at 0.3–0.5, H3 at 0.1–0.2, rest at 0.
- **Classic analog sub-bass:** H2 at 0.4, H4 at 0.2, rest minimal.
- **Aggressive/growl:** H3+H5 up, Step pushed to 0.5–0.7.
- **Hollow/woody:** H3 high, H2 low, H4 minimal.
- **Dense/full:** H2+H3+H4 each at 0.3–0.4.

### Step (Waveshape)
Step pushes the waveform from sine toward square via tanh saturation. At 0 it's a clean sine; at 1.0 it's nearly a square wave.

- 0.0–0.2: smooth, round harmonic content
- 0.3–0.5: warm overdrive character
- 0.6–1.0: harsh, buzzy — use with intent

Low Step + rich H2–H8 recipe sounds more "organic." High Step + sparse recipe sounds more "synth."

### Ghost Mix
Ghost blends in the original (unprocessed) bass signal alongside the synthesized output.

- Adding 20–40% ghost keeps the original attack and transient character while the synth adds harmonic weight underneath.
- On picked or plucked bass, ghost prevents the attack click from disappearing.
- On synth bass you usually want ghost near 0 — the original signal has no transients to preserve.

### Envelope (Attack / Release)
The envelope follower shapes how the synthesized output tracks the original signal's amplitude.

- **Fast attack + slow release:** The synth responds quickly to new notes but doesn't cut off abruptly on decay. Most musical for bass guitar.
- **Slow attack:** Creates a swell effect — good for pads or layered synth textures, not for tight bass playing.
- **Very fast release:** Can cause pumping artifacts on sustained notes. Keep release at 80 ms minimum for clean results.

### Sub Harmonic
The sub control synthesizes one octave below the detected fundamental. At low levels (0.1–0.25) it adds weight on speakers that can handle it. At higher levels it can overwhelm the mix.

- Best used to reinforce notes that are already present, not to create new sub content that wasn't there.
- Always check on a subwoofer or headphones before committing — sub content is invisible on smaller monitors.

### Output Gain
Keep the output meter below -3 dBFS to leave headroom for the dynamics of transient-heavy source material. Clipping from the plugin output is its own source of crackle.

---

## Quick Reference: Clean Bass

| Parameter         | Safe Range for Clean Sound |
|-------------------|---------------------------|
| Phantom Threshold | 35–60 Hz for bass guitar   |
| Gate Threshold    | Just above noise floor     |
| Tracking Speed    | 0.15–0.25                  |
| Wavelet Length    | 0.85–1.0                   |
| H1                | 0.8–1.0                    |
| Step              | 0.0–0.3                    |
| Ghost             | 20–40% for live bass        |
| Boost Amount      | 0–0.5                      |
| Env Release       | 80 ms minimum               |
| Output Gain       | ≤ −3 dBFS                  |

---

## Quick Reference: Common Problems

| Symptom                          | Likely Cause               | Fix                                      |
|----------------------------------|----------------------------|------------------------------------------|
| Stuttering on soft notes          | Gate too high              | Lower Gate Threshold                     |
| Crackle on note attacks           | Boost Amount too high      | Lower Boost Amount or raise Threshold    |
| Harsh/gritty tone on transients   | Tracking Speed too fast    | Slow down Tracking Speed                 |
| Thin or weak harmonics            | H1 too low                 | Raise H1 to 0.8–1.0                      |
| Pumping on sustained notes        | Boost + fast Env Release   | Lower Boost, increase Release time       |
| Pitch tracking drifts on low notes| Threshold too low          | Raise Phantom Threshold to 40+ Hz        |
| Choppy/rhythmic silences          | Wavelet Length too short   | Raise Length toward 1.0                  |
| Muddy low end                     | Sub too high               | Cut Sub below 0.2                        |
