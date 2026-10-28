# A350 BTV – Brake To Vacate, OANS & ROW/ROP for the FlightFactor A350

A FlyWithLua script for **X-Plane 12** that adds the A350's airport map (**OANS**), **Brake To Vacate (BTV)** and
**Runway Overrun Warning / Protection (ROW/ROP)** to the **FlightFactor A350**. Everything is drawn directly into the
3D cockpit displays and operated from the ND, as in the real aircraft.

Version 2.14.3 · Windows · MIT License

> Not affiliated with, endorsed by or supported by Airbus or FlightFactor. For flight simulation only.

---

## Features

- **OANS airport map on the ND**: runways, taxiways, exits, stands with numbers, ground markings, service roads and
  buildings (from OpenStreetMap). The map range is 0.2 / 0.5 / 1 / 2 / 5 NM, as on the A350.
  Menus and tabs as on the A350 (MAP DATA, ARPT SEL, LANDING, STATUS), plus crosses and flags.
- **BTV**: pick an exit on the map, arm BTV, and after touchdown the script brakes so that you reach the exit at
  10 kt, braking as late as possible like the real system. The FMA shows **BRK BTV** (cyan when armed, green when braking).
- **ROW / ROP** using the runway condition from the OANS LANDING page:
  - DRY / WET stop lines on the ND.
  - Messages on the PFD: RWY TOO SHORT, MAX BRAKING and MAX REVERSE.
  - Aural alerts: RUNWAY TOO SHORT, BRAKE MAX BRAKING, SET MAX REVERSE, KEEP MAX REVERSE and AUTO BRAKE OFF.
- **AUTO BRK OFF**: when the crew disconnects BTV, the aircraft's own MASTER CAUT light and chime come on.
- **RWY AHEAD** warning on the ground and **traffic** on the OANS map (AI traffic, LiveTraffic, xPilot …).
- **Automatic data**: exits and runways are read from your installed sceneries (`apt.dat`), the destination comes
  from the FMS, and the runway condition can be set to AUTO (from the weather).

---

## Requirements

| | |
|---|---|
| Simulator | X-Plane 12 on **Windows** (tested with 12.4.3, Vulkan) |
| Aircraft | FlightFactor A350 for X-Plane 12 |
| Plugin | **FlyWithLua NG+ 2.8 or newer** (tested with 2.8.12) – https://github.com/X-Friese/FlyWithLua/releases |

macOS and Linux are not supported in this release, because the helper plugin is only built for Windows.

---

## Installation

1. Install **FlyWithLua NG+**, if you don't have it yet.
2. Open this download. Copy the **`Resources`** folder into your **X-Plane 12 main folder** (the folder that
   contains `X-Plane.exe`). Windows merges it with the existing `Resources` folder. If Windows asks whether to
   replace files, choose **Replace**.

   After copying, the following files should exist:
   ```
   X-Plane 12/Resources/plugins/zz_A350_BTV_bridge/64/win.xpl
   X-Plane 12/Resources/plugins/FlyWithLua/Scripts/A350_BTV.lua
   X-Plane 12/Resources/plugins/FlyWithLua/Scripts/A350_BTV_data/   (font + sounds)
   ```
3. **Restart X-Plane**. X-Plane only loads the helper plugin `zz_A350_BTV_bridge` at startup.
4. Load the FlightFactor A350. The script only runs in the A350, so all other aircraft are unaffected.

On the first start the script builds an airport index from your sceneries (`A350_BTV_index.txt`). This takes a
few seconds once and is repeated automatically when your sceneries change.

### Updating

Copy the new `Resources` folder over the old one in the same way. Your settings are kept, because they are
stored in `A350_BTV_config.txt`.

### Uninstalling

1. Delete the folder `Resources/plugins/zz_A350_BTV_bridge`.
2. In `Resources/plugins/FlyWithLua/Scripts/`, delete:
   - `A350_BTV.lua`
   - the folder `A350_BTV_data`
   - all files whose names start with `A350_BTV_` (settings, index, download helpers).
3. If you used the optional step under Troubleshooting, also delete `zz_A350_BTV_bridge` from the A350's
   `plugins` folder.

---

## Quick start

### 1. Show the OANS map

The map only appears when the ND is powered, has a valid heading and the IRS are fully aligned, as on the real
aircraft. To show it:

- Turn the **ND range knob to 10**.
- Press the **OANS** key at the top right of the ND to switch the map on or off.

On the ground the map works in every EFIS mode. In flight it is available in NAV and PLAN. Below 500 ft on final
in ARC, it appears by itself.

**Views** (selected with the EFIS mode on the FCU):

| Mode | View |
|---|---|
| ARC | Taxi view: track up, follows the aircraft |
| ROSE | Track up, aircraft in the center. Dragging pans the map while the mouse button is held. |
| PLAN | North up, can be moved freely. In flight this is the BTV view (landing runway up). |
| NAV (in flight) | Airport map, north up |

**Map controls:**

- **Mouse wheel**: changes the map range.
- **Double-click**: opens the context menu (ADD CROSS, ADD FLAG, MAP DATA, CENTER ON ACFT …).
- **Double arrow at the bottom right**: opens the panel with four tabs:

| Tab | Content |
|---|---|
| MAP DATA | Find a runway, taxiway or stand; set crosses and flags; CENTER MAP ON |
| ARPT SEL | Select an airport by ICAO, IATA or city |
| LANDING | Landing runway, **RWY COND** (AUTO = from the weather, or 6 DRY … 1 POOR), BTV stop distance, BTV ARM / DISARM |
| STATUS | Data sources, brakes, EFIS mode |

The airport is selected automatically: in flight it is the FMS destination, on the ground the nearest airport.

### 2. Arm BTV (in flight)

1. Set the A350's own **autobrake to OFF**. As long as it is on, BTV ARM is blocked.
2. Open the OANS map, for example in PLAN mode. The FMS landing runway is selected automatically (green
   triangle at its name). To use another runway, tap its name (QFU) at the runway end.
3. Choose where to stop:
   - Tap an **exit** and choose **SET BTV EXIT**. Green exits are recommended. Red exits are too short and can't
     be selected.
   - Or enter a **stop distance** on the LANDING tab.
4. Press **BTV ARM** on the LANDING tab, or use the command `arm_toggle`.

**BRK BTV** now appears in cyan in the FMA. If you change the runway, exit, stop distance, airport or FMS runway,
BTV is disarmed ("BTV DISARMED"), as on the A350.

### 3. Landing

- When the ground spoilers extend after touchdown, BTV starts braking. The FMA shows **BRK BTV** in green.
  If the nose gear is still in the air, BTV waits until it touches down, at most 5 seconds.
- The ND switches to the taxi view and zooms in step by step as you approach the exit.
- BTV ends at the exit speed. Brake further manually as usual.

**Disconnecting BTV:**

- Any of the following disconnects BTV:
  - braking: with rudder pedals (toe brakes), or with X-Plane's brake commands from a key or joystick button
    (for example **B** "Hold brakes regular", "Hold brakes maximum", "Left/Right brake")
  - DISARM, or the command `disconnect`
  - pushing the thrust levers forward
  - retracting the ground spoilers
- You don't need rudder pedals: BTV applies the brakes itself. Without pedals, use the brake key or one of the
  other ways above to take over.
- After a disconnect you get **AUTO BRK OFF** on the PFD, the aircraft's MASTER CAUT light and chime, and the
  callout "AUTO BRAKE OFF".
- Tip: bind `disconnect` to the A/THR instinctive-disconnect button on your throttle.
- A **go-around** (TOGA) only disarms BTV, without any warning.

### 4. ROW / ROP

- **ROW** works on final below 500 ft, also when BTV is not armed. With BTV armed, the predicted stop lines
  are already shown on the BTV map before that. Below 500 ft they update in real time.
  - **WET** line beyond the runway end: "IF WET : RWY TOO SHORT" in amber.
  - **DRY** line beyond the runway end: "RWY TOO SHORT" in red. Below 200 ft you also hear "RUNWAY TOO SHORT".
- **ROP** is active after touchdown on the selected runway:
  - If even maximum braking would not stop the aircraft before the runway end, BTV brakes at maximum.
    The PFD shows **MAX REVERSE** and you hear "SET MAX REVERSE" until both reversers are fully deployed.
    Below 80 kt you hear "KEEP MAX REVERSE".
  - When braking manually or with the aircraft's autobrake: "BRAKE, MAX BRAKING" until full braking is applied
    (at least 80 % brake input), then the same reverse alerts as above.
- The stop lines use the runway condition from the LANDING tab (**RWY COND**). With DRY selected you see a DRY
  and a WET line. With any other condition you see one line labelled with that condition.

---

## Settings

Open the settings with **FlyWithLua menu → Macros → "A350 BTV: Settings"**, or bind the command `toggle_window`.
Options that work like the real A350 are marked **(REAL)**.

- **OANS:**
  - Map on/off.
  - Traffic on the map: *like TCAS* (TRAF selected, TCAS in TA or TA/RA), *always* or *off*.
- **Airport data:**
  - Download buildings from OpenStreetMap. This needs internet once per airport, and the result is cached in
    `A350_BTV_data/osm2_<ICAO>.json`.
  - Estimate terminal buildings from gate positions.
- **BTV:** exit speeds. The real A350 uses 10 kt about 50 m before the exit. **Set REAL** restores these values.
- **ROW / ROP:** on/off, aural alerts on/off, voice, volume and test buttons.
- **MASTER CAUT:** on BTV disconnect, with a test button.
- **Other:** status line at the top of the screen, layered ND drawing (uses less CPU) and a frame around the ND
  area (for checking the position).

### Voices and custom sounds

| Voice | Description |
|---|---|
| REAL | Recorded A350 callouts (default). KEEP MAX REVERSE is synthetic, matched to the recording. |
| Synthetic 1 (US), 2 (UK), 3 (US, deep) | Generated voices, filtered to sound like a cockpit speaker |

The callouts play through X-Plane's sound system on the cockpit (interior) channel, so the X-Plane "Interior"
volume slider applies.

To use your own sounds, create the folder `A350_BTV_data/sounds/custom/` and put files there with these names:
`runway_too_short`, `set_max_reverse`, `keep_max_reverse`, `brake_max_braking`, `auto_brake_off`
(`.wav`, `.ogg` or `.mp3`). Custom files take priority over the selected voice.

### Commands (for joystick / keyboard)

All commands are under `FlyWithLua/A350_BTV/`:

| Command | Function |
|---|---|
| `toggle_window` | Settings window show/hide |
| `arm_toggle` | BTV ARM / DISARM |
| `disconnect` | Disconnect BTV (bind to the A/THR disconnect button) |
| `next_exit` / `prev_exit` | Select the next / previous exit |
| `nd_zoom` / `nd_zoom_fo` | OANS map on/off on the captain / first officer ND |
| `nd_map_in` / `nd_map_out` | OANS map range in / out |

---

## Troubleshooting

All messages of the script are in X-Plane's `Log.txt` (X-Plane 12 main folder). Search for `A350_BTV`.

- **The OANS key or map does not appear.**
  - Check that the ND is powered, the IRS are fully aligned and the range knob is on 10.
  - In the settings window, make sure "OANS map on the ND" is enabled.
- **You press OANS, but the normal FF ND stays visible** (the map is hidden behind the ND).
  - X-Plane decides in which order plugins draw. Normally the global helper plugin draws late enough.
  - If not on your system, copy the folder `Optional/Aircraft bridge/zz_A350_BTV_bridge` into the `plugins`
    folder of your A350 (for example `Aircraft/addon aircraft/AirbusA350/plugins/`) and restart X-Plane.
  - The script tests both helper plugins by itself. Look for `ND map: draw order` in `Log.txt`.
- **No callouts.**
  - Check the X-Plane "Interior" volume and the volume slider in the settings window.
  - Use the Test button next to the voice in the settings window.
- **No buildings on the map.**
  - The OpenStreetMap download needs internet. It runs `curl` in the background through a small helper file
    (`A350_BTV_osm.bat` / `.vbs`) in the Scripts folder.
  - Some airports simply have no buildings in OpenStreetMap. Use "Reload" in the settings window to try again.
- **FlyWithLua stopped all scripts.** This means a script error. Please send the lines with `A350_BTV` from
  `Log.txt` when reporting the problem.
- **Crash protection.** If X-Plane crashes the first time the map is drawn, the next start switches off
  "Layered ND drawing". After another crash, the OANS map is switched off. You can switch both back on in the
  settings window.

---

## Credits and licenses

- **A350 BTV** (script and helper plugin): MIT License – see `LICENSE.txt`. The source code of the helper
  plugin is in `Source/`.
- **B612 Mono** font (the Airbus cockpit font): © The B612 Project Authors, SIL Open Font License 1.1. It is
  included as a pre-rendered bitmap atlas (`font_b612_px2.bin`); the license is in
  `A350_BTV_data/font_b612_OFL.txt`.
- **Synthetic voices**: generated with Kokoro-82M (Apache License 2.0) via sherpa-onnx (Apache License 2.0).
- **Building data**: © OpenStreetMap contributors, ODbL. Downloaded at runtime through the public Overpass API
  servers.
- Airbus, A350 and FlightFactor are trademarks of their respective owners. They are used here only to describe
  compatibility.

See `THIRD_PARTY_NOTICES.txt` for details.
