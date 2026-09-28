# Suppressor interface

The editor implements the shared [Affine design language](../libs/affine-ui/DESIGN_LANGUAGE.md)
(v2.0) in its **Hi-Fi** finish through the vendored `affine_ui` JUCE module. This
document records only the Suppressor-specific decisions.

<p align="center">
  <img src="images/suppressor-ui.png" alt="Suppressor in the Hi-Fi finish: a black glass front between polished end caps, a green lit wordmark, a row of status legends with SUPPRESSING lit, a blue-lit high-band reduction meter near 40 dB, input and output bar meters with readouts, three silver knobs and a Listen button" width="800">
</p>

## Product

- **Purpose:** zero-latency, frequency-split noise suppression for guitar DI before
  high-gain amplification.
- **Primary task:** remove high-band noise between notes without losing body, sustain or
  pick attack, and check what was removed when in doubt.
- **Finish:** a 1970s hi-fi front: black glass between polished aluminium end caps,
  legends printed on the back of the glass and lit, a blue-lit meter, silver knobs and a
  polished square push button.
- **Editor:** fixed 880 × 540 logical pixels; renders natively at 100–200% and on Retina.

## Layout

The instruments fill the top of the glass; a faint lit rule separates them from the
controls.

| Zone | Contents |
|---|---|
| Header | Lit wordmark and descriptor; a row of status legends (*No signal*, *Passing*, *Suppressing*, *Listen*, *Learn*, *Bypass*) of which the current one lights; *Host settings active* underneath when non-default legacy settings exist |
| Instruments | Blue-lit **reduction** meter; **INPUT** and **OUTPUT** bar meters (the output relabelled **REMOVED** while auditioning), each with a peak readout and a clip cell |
| Controls | Threshold, Strength, Release: medium silver knobs with calibrated scales and glass readouts; *Listen removed* as a polished push button whose legend lights amber while engaged |

## Telemetry contract

All readings come from the processor's lock-free `MeterSnapshot`. A reading is live only
when a *new* audio block arrived within 500 ms and its routing flags match the current
Listen and band-mode settings, so an old block is never relabelled as the new mode.

| Display | Source | Presentation |
|---|---|---|
| INPUT / OUTPUT bars | Peak across channels, 300 ms decay | −60 to 0 dBFS in 2 dB segments: blue, amber from −12 dBFS, red from −3 dBFS |
| Clip cells | Processor clip flags, held for one second | Separate red cell at the end of each bar |
| Peak readouts | Same peaks | dBFS to 0.1 dB, `-inf` for silence, `--` when not live |
| Reduction meter | Deepest high-band gain reduction, 120 ms decay | 0–60 dB on a square-root scale so small reductions stay readable; the lamps dim and the needle rests when reduction is unavailable (no audio, no input, learning) |
| Status legends | Snapshot flags | *No audio* and *No input* light **No signal**; the other states light their own legend |

The needle's spring-damper ballistics (about 300 ms to settle) are presentation only.

## States

| State | Presentation |
|---|---|
| Suppressing | **Suppressing** lit green, needle on the reduction, bars live |
| Listen removed | **Listen** lit amber, the button's legend lit amber, the output bar reads **REMOVED** with an amber readout |
| Multiband topology | Meter caption reads **MAX BAND REDUCTION**; Threshold and Strength are disabled because that mode uses its own splits and learned thresholds |
| Learning bands | **Learn** lit amber; reduction marked unavailable, so the meter lamps dim |
| Bypassed | **Bypass** lit red; needle at rest with the lamps lit (no reduction is applied) |
| Stopped audio | **No signal** lit; meter lamps dim, needle rests, bars dark, readouts `--` |

<p align="center">
  <img src="images/suppressor-delta-audition.png" alt="Listening to the removed signal: Listen legend and button lit amber, output bar relabelled REMOVED" width="400">
  <img src="images/suppressor-learning.png" alt="Learning bands in multiband mode: Learn legend lit amber, dimmed reduction meter, disabled Threshold and Strength, host settings line" width="400">
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
- *Listen removed* toggles with a click, Space or Return, as one host gesture.
- Tab order: Threshold, Strength, Release, Listen removed. Focus shows as an illuminated
  ring at the foot of the knob or an outline around the button.

<p align="center">
  <img src="images/suppressor-exact-entry.png" alt="Exact entry: the Strength readout becomes a text field" width="400">
</p>

## Accessibility

- Every control exposes its parameter name, formatted value with unit, and a description.
- The *Processing status* legends are one label whose text is the full state; the
  *Signal meters* label carries a live summary (input and output dBFS, reduction or why
  it is unavailable, clip warnings). The peak readouts are hidden from assistive
  technology so values are not announced twice.
- State is never colour-only: each state has its own printed legend, and every mode
  change has a printed or displayed label.
- Printed legends meet 4.5:1 on the glass, including its darker lower edge; emitted
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
