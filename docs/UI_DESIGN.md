# Suppressor interface

The editor implements the shared [Affine design language](../libs/affine-ui/DESIGN_LANGUAGE.md)
(v1.0) through the vendored `affine_ui` JUCE module. This document records only the
Suppressor-specific decisions.

<p align="center">
  <img src="images/suppressor-ui.png" alt="Suppressor suppressing: live input and output ladders, backlit high-band reduction meter near 40 dB, and the three suppression knobs" width="800">
</p>

## Product

- **Purpose:** zero-latency, frequency-split noise suppression for guitar DI before
  high-gain amplification.
- **Primary task:** remove high-band noise between notes without losing body, sustain or
  pick attack, and check what was removed when in doubt.
- **Finish:** graphite anodising, glacier-cyan emission, an incandescent meter card.
- **Editor:** fixed 800 × 530 logical pixels; renders natively at 100–200% and on Retina.

## Layout

The signal runs left to right across the meter bridge, and the controls sit below it.

| Zone | Contents |
|---|---|
| Header | Wordmark; processing-status display with lamp; *Host settings active* lamp underneath when non-default legacy settings exist |
| Meter bridge | **IN** peak ladder → backlit **reduction** meter → **OUT** peak ladder (relabelled **REMOVED** while auditioning), each ladder with a clip cell and a peak readout |
| Suppression frame | Threshold, Strength, Release: medium knobs with calibrated scales and glass readouts |
| Monitor frame | *Listen removed* latching key with an amber LED |

## Telemetry contract

All readings come from the processor's lock-free `MeterSnapshot`. A reading is live only
when a *new* audio block arrived within 500 ms and its routing flags match the current
Listen and band-mode settings, so an old block is never relabelled as the new mode.

| Display | Source | Presentation |
|---|---|---|
| IN / OUT ladders | Peak across channels, 300 ms decay | −60 to 0 dBFS in 3 dB segments: cyan, amber from −12 dBFS, red from −3 dBFS |
| Clip cells | Processor clip flags, held for one second | Separate red cell above each column |
| Peak readouts | Same peaks | dBFS to 0.1 dB, `-inf` for silence, `--` when not live |
| Reduction meter | Deepest high-band gain reduction, 120 ms decay | 0–60 dB on a square-root scale so small reductions stay readable; lamp dims and needle rests when reduction is unavailable (no audio, no input, learning) |
| Status | Snapshot flags | *No audio*, *No input*, *Passing signal*, *Suppressing*, *Listening to difference*, *Bypassed*, *Learning bands* |

The needle's spring-damper ballistics (about 300 ms to settle) are presentation only.

## States

| State | Presentation |
|---|---|
| Suppressing | Cyan status lamp, needle on the reduction, ladders live |
| Listen removed | Amber key LED and status, **REMOVED** legend and amber readout on the output column |
| Multiband topology | Meter caption reads **MAX BAND REDUCTION**; Threshold and Strength are disabled (veiled) because that mode uses its own splits and learned thresholds |
| Learning bands | Amber status; reduction marked unavailable, so the meter lamp dims |
| Bypassed | Needle at rest with the lamp lit (no reduction is applied) |
| Stopped audio | Lamp dims, needle rests, ladders dark, readouts `--` |

<p align="center">
  <img src="images/suppressor-delta-audition.png" alt="Listening to the removed signal: amber status, lit Listen key and REMOVED output column" width="400">
  <img src="images/suppressor-learning.png" alt="Learning bands in multiband mode: amber status, dimmed meter, disabled Threshold and Strength, host settings lamp" width="400">
</p>

## Parameter contract

Unchanged: all 20 parameters keep their IDs, order, version hints, ranges, defaults,
tapers, text conversion and automation. The editor exposes Threshold, Strength, Release
and Delta Audition; the remaining engine settings stay available in the host's parameter
view and are summarised by the *Host settings active* lamp's tooltip.

## Interaction

- Knobs follow the family contract: drag anywhere (up or right increases; 240 px for the
  full range, Shift for ten times finer), double-click or Return for exact entry in the
  readout (Return commits, Escape cancels, malformed text never changes the value),
  Alt/Option-click or Home to reset, arrows to step (Strength 1%, Threshold and Release
  0.1), right-click for *Enter value*, *Reset to default* and the host's parameter menu.
- *Listen removed* toggles with a click, Space or Return, as one host gesture.
- Tab order: Threshold, Strength, Release, Listen removed. Focus shows as an illuminated
  ring at the foot of the knob or an outline around the key.

<p align="center">
  <img src="images/suppressor-exact-entry.png" alt="Exact entry: the Strength readout becomes a text field" width="400">
</p>

## Accessibility

- Every control exposes its parameter name, formatted value with unit, and a description.
- The *Signal meters* label carries a live summary (input and output dBFS, reduction or
  why it is unavailable, clip warnings); the per-column readouts are hidden from assistive
  technology so values are not announced twice.
- State is never colour-only: every lamp has a text legend and every mode change has a
  printed or displayed label.
- Printed text meets 4.5:1 on the plate, including its darker lower edge; emitted
  colours meet 4.5:1 on display glass. Both are unit-tested.

## Verification

- `SuppressorUITests` (doctest, 8 cases) covers bundled fonts and contrast, the complete
  parameter and saved-state contract, balanced drag, fine, exact, invalid, reset, keyboard
  and close gestures, truthful live, stale, bypass, learning and audition states, clip
  reporting, rendering at 100–200%, and the native VST3 and CLAP editor lifecycle.
- The DSP suite is unchanged and passes.
- pluginval 1.0.4 at strictness 10 reports `SUCCESS` for the VST3.
- With `SUPPRESSOR_UI_CAPTURE_DIR` set, the UI tests write a PNG of every state and scale;
  CI uploads them for macOS, Windows and Linux.
