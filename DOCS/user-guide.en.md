# ESDR3_Player. User Guide

## What it is

A player for ExpertSDR3 IQ recordings. It opens files named
`ExpertSDR3_IQ_Freq_<Hz>_Date_<date>_Time_<time>.wav`, shows the spectrum and waterfall of the
whole recorded band, lets you tune to any station inside it and listen in CW, USB, LSB, AM or FM.
Runs on macOS, Windows and Linux.

The sample rate, centre frequency (DDS) and recording start time come from the `esdr` chunk inside
the file, so the frequency scale shows real frequencies and the waterfall labels show real
recording time.

## Installation

- **macOS**: open the `.dmg` and drag `ESDR3_Player.app` to Applications. The build is ad-hoc
  signed, so the first launch may be blocked: right-click the app and choose "Open".
- **Windows**: unpack the `.zip` anywhere and run `ESDR3_Player.exe`.
- **Linux**: make the `.AppImage` executable and run it. `libfuse2` is required.

## Window

- **Top bar**: Open, Play/Pause, Stop, Loop, speed (0.5x–4x), file name, recording time at the
  current position, position and duration.
- **Panorama**: spectrum in dB full scale at the top, the frequency scale in kHz below it, then the
  waterfall with recording-time labels on the left. The yellow line is the VFO, the translucent
  band is the filter passband.
- **Right panel**: receiver, spectrum, waterfall, language.
- **Bottom**: position slider and state.

## Mouse on the panorama

| Action | Result |
|---|---|
| Click | VFO to that frequency (10 Hz steps, 1 Hz with Shift) |
| Wheel | Zoom around the cursor, from the full band down to 1 kHz |
| Drag the background | Pan the visible part of the band |
| Drag the VFO line | Continuous tuning |
| Drag a filter edge | Change the filter bandwidth |
| Double click | Show the whole recorded band |

## Keys

| Key | Action |
|---|---|
| Space | Pause / resume |
| ← → | Seek ±5 s, ±60 s with Shift |
| Home | Back to start |
| ↑ ↓ | VFO ±10 Hz, ±100 Hz with Shift |
| M | Mute / unmute |
| + − 0 | Zoom in, zoom out, full band |
| Cmd/Ctrl+O | Open file |

While the cursor sits in the Width or Pitch field, the single-key shortcuts from the table do
nothing: the keystrokes go into the number.

## Receiver

- **Frequency**: the digits above the mode buttons. Point at a digit and turn the wheel: the step
  equals that digit's weight, the last one gives 1 Hz. Tuning stays inside the recorded band.
- **Modes**: CW, USB, LSB, AM, FM. The bandwidth is remembered per mode.
- **Width**: total filter width. For CW, AM and FM the filter is symmetric around the carrier;
  for USB and LSB it starts 200 Hz from the carrier.
- **Pitch**: CW tone, 300–1200 Hz. A station whose carrier sits on the VFO sounds at this tone.
- Width and Pitch can also be typed: click the field next to the slider and enter the number.
  Enter applies, Esc cancels. A value outside the allowed range is not accepted.
- **AGC**: Off (manual gain), Fast, Slow.
- **Volume**, **Mute**, **Output** (audio device). When the system's audio devices change, the
  output switches automatically.
- Audio is produced only at 1x speed. At other speeds the panorama keeps working, audio is off.

## Spectrum and waterfall

- **FFT**: 4096–32768. More points means narrower bins and slower response.
- **Average**: time smoothing of the spectrum.
- **Top/Bottom**: spectrum scale limits in dB.
- **Auto range**: the waterfall picks its floor and ceiling from the noise level of each line.
  Untick it to set the limits by hand.
- **Palette**, **Height** (waterfall share of the panorama), **Clear waterfall**.

## Files

- Recordings of any length are streamed from disk; memory use does not grow with file size.
- A file that ExpertSDR3 is still writing can be opened: when the player reaches the end, it
  picks up whatever has been appended.
- Plain IQ WAV files from other programs (PCM 16/24/32 bit, float32) also open, but without a
  centre frequency: the scale starts at zero.

## Command line

```
ESDR3_Player [file.wav] [--play] [--mute] [--lang en|ru]
```

## Troubleshooting

- No audio: check Output and Mute, look at the "Audio … underruns" line. A growing underrun
  count means the computer cannot keep up; reduce the FFT size or frame rate.
- A CW station sounds at the wrong tone: put the VFO exactly on the station's line by dragging
  the yellow line.
- A file does not open: the error appears in red in the right panel.
