# Suppressor interface

The editor implements the shared [Affine design language](../libs/affine-ui/DESIGN_LANGUAGE.md)
(v2.0) in its **Broadcast** finish through the vendored `affine_ui` JUCE module. This
document records only the Suppressor-specific decisions.

<p align="center">
  <img src="images/suppressor-ui.png" alt="Suppressor in the Broadcast finish: a hammertone rack panel with an engraved name plate, a green SUPPRESSING status window, input and output VU meters around a large high-band reduction meter near 40 dB, three bakelite knobs and a chrome Listen toggle" width="800">
</p>

## Product

- **Purpose:** zero-latency, frequency-split noise suppression for guitar DI before
  high-gain amplification.
- **Primary task:** remove high-band noise between notes without losing body, sustain or
  pick attack, and check what was removed when in doubt.
- **Finish:** a 1960s studio rack panel: warm grey hammertone enamel between rack ears,
  a riveted name plate, bakelite knobs, chrome-bezel VU meters with amber cards, jewel
  lamps and amber backlit readouts.
- **Editor:** fixed 880 × 560 logical pixels; renders natively at 100–200% and on Retina.

## Layout

The signal runs left to right across the meter bridge, and the controls sit below it.

| Zone | Contents |
|---|---|
| Header | Engraved name plate and descriptor; backlit status window lit in the state's colour; *Host settings active* line underneath when non-default legacy settings exist |
| Meter bridge | **INPUT** VU meter → large **reduction** meter → **OUTPUT** VU meter (relabelled **REMOVED** while auditioning), each small meter with a clip jewel |
| Suppression frame | Threshold, Strength, Release: medium bakelite knobs with calibrated scales and backlit readouts |
| Monitor frame | *Listen removed* as a chrome bat-handle toggle: up is **REMOVED**, down is **PROCESSED** |

## Telemetry contract

All readings come from the processor's lock-free `MeterSnapshot`. A reading is live only
when a *new* audio block arrived within 500 ms and its routing flags match the current
Listen and band-mode settings, so an old block is never relabelled as the new mode.

| Display | Source | Presentation |
|---|---|---|
| INPUT / OUTPUT meters | Peak across channels, 300 ms decay | dBFS, printed −40 to 0 with the scale opened up towards 0; red zone from −6 dBFS |
| Clip jewels | Processor clip flags, held for one second | Red jewel under each meter |
| Reduction meter | Deepest high-band gain reduction, 120 ms decay | 0–60 dB on a square-root scale so small reductions stay readable; lamp dims and needle rests when reduction is unavailable (no audio, no input, learning) |
| Status window | Snapshot flags | *No audio*, *No input*, *Passing signal*, *Suppressing*, *Listening to difference*, *Bypassed*, *Learning bands* |

Needle ballistics are presentation only: the reduction meter moves like a VU meter
(about 300 ms to settle); the level meters are quicker so peaks stay visible.

## States

| State | Presentation |
|---|---|
| Suppressing | Green status window, needle on the reduction, level meters live |
| Listen removed | Toggle up; amber status window; the output meter reads **REMOVED** |
| Multiband topology | Meter caption reads **MAX BAND REDUCTION**; Threshold and Strength are disabled because that mode uses its own splits and learned thresholds |
| Learning bands | Amber status window; reduction marked unavailable, so the meter lamp dims |
| Bypassed | Red status window; reduction needle at rest with the lamp lit (no reduction is applied) |
| Stopped audio | Status window unlit, meter lamps dim, needles rest |

<p align="center">
  <img src="images/suppressor-delta-audition.png" alt="Listening to the removed signal: amber status window, toggle up and the output meter relabelled REMOVED" width="400">
  <img src="images/suppressor-learning.png" alt="Learning bands in multiband mode: amber status window, dimmed reduction meter, disabled Threshold and Strength, host settings line" width="400">
</p>

## Parameter contract

Unchanged: all 20 parameters keep their IDs, order, version hints, ranges, defaults,
tapers, text conversion and automation. The editor exposes Threshold, Strength, Release
and Delta Audition; the remaining engine settings stay available in the host's parameter
view and are summarised by the *Host settings active* tooltip.

## Interaction

- Knobs follow the family contract: drag anywhere (up or right increases; 240 px for the
  full range, Shift for ten times finer), double-click or Return for exact entry in the
  readout (Return commits, Escape cancels, malformed text never changes the value),
  Alt/Option-click or Home to reset, arrows to step (Strength 1%, Threshold and Release
  0.1), right-click for *Enter value*, *Reset to default* and the host's parameter menu.
- The *Listen removed* toggle flips with a click, Space or Return, as one host gesture.
- Tab order: Threshold, Strength, Release, Listen removed. Focus shows as an illuminated
  ring at the foot of the knob or an outline around the toggle.

<p align="center">
  <img src="images/suppressor-exact-entry.png" alt="Exact entry: the Strength readout becomes a text field" width="400">
</p>

## Accessibility

- Every control exposes its parameter name, formatted value with unit, and a description.
- The *Signal meters* label carries a live summary (input and output dBFS, reduction or
  why it is unavailable, clip warnings).
- State is never colour-only: the status window always spells the state out, and every
  mode change has a printed or displayed label.
- Printed ink meets 4.5:1 on the enamel, including its darker lower edge; the dark ink of
  the readouts and the status window meets 4.5:1 on each of their lit colours. Both are
  unit-tested.

## Verification

- `SuppressorUITests` (doctest, 8 cases) covers bundled fonts and contrast, the complete
  parameter and saved-state contract, balanced drag, fine, exact, invalid, reset, keyboard
  and close gestures, truthful live, stale, bypass, learning and audition states, clip
  reporting, rendering at 100–200%, and the native VST3 and CLAP editor lifecycle.
- The DSP suite is unchanged and passes.
- pluginval 1.0.4 at strictness 10 reports `SUCCESS` for the VST3.
- With `SUPPRESSOR_UI_CAPTURE_DIR` set, the UI tests write a PNG of every state and scale;
  CI uploads them for macOS, Windows and Linux.
