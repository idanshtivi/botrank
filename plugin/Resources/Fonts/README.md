# Embedded UI typefaces

Three families, one role each (see `Source/Typography.h`). Archivo and IBM
Plex Mono are SIL Open Font License 1.1 — free to embed in a commercial
binary, no runtime attribution required. Satoshi is distributed under
Fontshare's free license (see `LICENSE-Satoshi.txt`) — also free to embed,
just not to resell as a standalone font product. License text for each
family is included alongside its files.

## Title / brand — Big Shoulders Display ExtraBold

- `BigShouldersDisplay-ExtraBold.ttf` — Google Fonts, SIL OFL 1.1. A
  condensed industrial slab with a stamped-metal-nameplate feel, closer to
  real vintage hardware synth panels than a general-purpose sans. Static
  wght=800 instance produced with `fonttools varLib.instancer
  --update-name-table` from Google Fonts' variable
  `BigShouldersDisplay[wght].ttf`, since Google Fonts does not publish a
  pre-built static ExtraBold file.
- `LICENSE-BigShouldersDisplay.txt`
- `Archivo-ExpandedBold.ttf` — static fallback if the above ever fails to
  load. A genuine static instance (wght=700, wdth=125) produced the same
  way from Google Fonts' official variable `Archivo[wdth,wght].ttf`.
  Renamed internally to family "Archivo Expanded", style "Bold".
- `ArchivoBlack-Regular.ttf` — second-tier fallback if Archivo Expanded Bold
  also fails to load.
- `LICENSE-Archivo.txt`

## Panel / knob labels — Satoshi

Substituted for IBM Plex Sans Condensed per product direction — Satoshi
reads cleaner at small panel-label sizes and ships a genuine italic cut.
Sourced from Fontshare (fontshare.com/fonts/satoshi). Satoshi has no
dedicated SemiBold cut, so `Typography::Weight::SemiBold` falls back to Bold
for this family (see `Typography.cpp`).

- `Satoshi-Regular.otf`
- `Satoshi-Medium.otf`
- `Satoshi-Bold.otf`
- `Satoshi-Italic.otf`
- `LICENSE-Satoshi.txt`

## Numeric displays / readouts — IBM Plex Mono

- `IBMPlexMono-Regular.ttf`
- `IBMPlexMono-Medium.ttf`
- `IBMPlexMono-SemiBold.ttf`
- `IBMPlexMono-Bold.ttf`
- `LICENSE-IBMPlexMono.txt`

## Build

`plugin/CMakeLists.txt` globs every `.ttf` and `.otf` in this folder into the
`LadderVoiceFonts` binary-data target automatically — no CMake changes are
needed when adding/removing files here. If this folder is ever emptied,
`Typography.cpp` falls back to platform system fonts per family so the
build and app still work, just without the intended typefaces.
