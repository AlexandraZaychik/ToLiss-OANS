# Magknight 787 experimental adapter

The adapter targets the inspected Magknight 787-8, 787-9 and 787-10 Aviators
XP12 1.11.9 at the same experimental feature level. Other vendors' 787s are
not supported. It identifies the ACF by author, model name and ICAO, regardless
of the ACF filename. The inspected 787-10 declares B789; its 787-10 model name
selects `b78x.json` rather than `b789.json`. A corrected B78X ICAO is accepted
for the same model name. Inconsistent or unknown identities fail closed.

All three ACFs attach `common/cockpit/obj/cmn_cockpit_1.obj` without rotation
or translation. They therefore share the display UVs, pane seam and aspect,
but use separate CG-relative click profiles:

| Profile | Authored ICAO | CG Y / Z (feet) | Click offset relative to B788 (metres) |
| --- | --- | --- | --- |
| b788.json | B788 | -10.710000038 / 87.769996643 | none |
| b789.json | B789 | -10.710000038 / 97.769027710 | Z -3.04770467 |
| b78x.json | B789 | -0.709999979 / 87.769996643 | Y -3.04800002 |

The adapter loads the canonical model's profile on aircraft reload, including
switches between the two ACFs declaring B789. A missing or invalid profile
clears old geometry; it never falls back to B788. Diagnostics print
`MAGKNIGHT_B788`, `MAGKNIGHT_B789` or `MAGKNIGHT_B78X` separately from the
simulator's raw ICAO.

The first OANS opening selects the nearest eligible airport within 40 NM of
the aircraft. The selected airport is retained during flight; each observed
airborne-to-ground transition selects the nearest eligible airport again,
including after a go-around and a subsequent landing. Touchdown also replaces a
manual airport selection. If position or airport data is not ready, selection
waits for it; if no eligible airport is within 40 NM, the map is unavailable.
The NEAREST command also returns to automatic nearest-airport selection.

Both inboard displays are enabled experimentally. Each OANS command cycles
through three states, starting off:

1. Off: the aircraft's display remains visible.
2. Circle: OANS covers only the active ND half of the MFD, keeping its other half.
3. Check mark: OANS covers the full inboard MFD by merging both halves.

The outboard PFD and the separate lower CDU are outside both OANS layouts.
Map geometry, symbols and text preserve physical proportions based on the 3-D
glass dimensions (approximately 0.666 half / 1.331 full width-to-height). The
rectangular viewport and compass radius adapt to each mode; picking and dragging
use the inverse of the same horizontal compensation.
The active half follows defaultNavDisplays[0]/[1] for the captain and [4]/[5] for the first officer. The FO indices are provisional and require simulator verification.
The label-free switch sits at the lower outside corner of the active ND side,
including in full display. Clicking it toggles between circle (half) and check
(full). It is hidden while OANS is off; automatic range control and the corresponding side's
command can still turn OANS off.

The profile stores the full display UV rectangle and corners. Rendering, blanking,
and mouse projection select the active pane or the full display together.
Geometry comes from the shared cockpit object's adjacent panel quads, with
the aircraft CG offset applied to click coordinates. The FO display uses its own measured OBJ geometry and UVs. PFD geometry is a schema placeholder and advisory rendering remains disabled.

The full-display header follows the reference's compact arrangement: ground
speed, true airspeed, boxed OANS map range, MAP/PLAN indication, magnetic ground
track when heading-up (heading is indicated if track is unavailable), and a
boxed airport identifier. The bottom panel-toggle arrow is hidden in full mode.
MAP, PLAN and MENU are visual button
placeholders; MAP/PLAN highlights the physical selector and none is clickable.
On the ground, the physical selector at 5 or 2.5 NM automatically opens the
both OANS displays at 5 NM. The aircraft selector is returned to 5 NM while OANS
is controlling it. Turns inward or outward step the OANS range through 5, 2,
1 and 0.5 NM. Turning outward from OANS 5 NM exits to the aircraft's 10 NM
display; returning to 5 NM opens OANS again. Takeoff closes automatic OANS.
This behavior requires a writable selector and has not yet been verified in
the simulator. The OANS range can differ from the physical ND range. Both half and full MAP use the inward-facing 787 compass;
Full-display PLAN uses two solid range rings, labelled at half and quarter of
the selected range, and a north-up indicator. Its bottom MAP CENTERING strip
allows AIRPLANE to restore ownship following; DEST, CURSOR, CTR ON and PICK WPT
are visual placeholders. Dragging the map still changes the centre. Half-display
PLAN retains the shared compass. The 787 map colours and labels are
adapted separately: runways, aprons and stopway surfaces are shown at every
supported range; terminal footprints, taxiway identifiers and runway-end
identifiers are limited to 2, 1 and 0.5 NM; cyan gate identifiers appear at
0.5 NM. Chevrons are suppressed at 5 NM.
Terminal footprints depend on available building data. The 787 airport
location star uses the airport's accepted tower viewpoint, as specified by the
user, and appears in white at 2, 1 and 0.5 NM. If no suitable tower viewpoint
is available the symbol is omitted. The 787 layer omits A350 ground markings.
WXR ON, holding lines, construction, closed-runway,
de-icing and helipad symbols need dedicated data and implementation. In half mode OANS covers the entire selected ND pane and draws a MAP/PLAN/MENU header matched to the split-screen reference. These header buttons remain visual placeholders. The half-display header and shallow compass layout follow the split-screen reference. Bottom controls
still use the shared A350 implementation. This
visual pass needs simulator validation.

During an earlier read-only sample from the running 787-8, map mode was 2, range
was 20 NM, defaultNavDisplays[0] was 1, IRS aligned was 1, and the inboard
brightness control was 5. The experimental overlay currently requires this
navigation flag, IRS alignment, brightness, MAP/PLAN and a positive physical ND
range of 5 NM or less. Above 5 NM, both the switch and the overlay are hidden;
returning to 5 NM or less restores the selected OANS state. The
meaning of each flag across display switching, PLAN, power, and brightness
settings is not yet verified. Rendering order, click projection, screen aspect
ratio, and interaction with the aircraft's SASL drawing need in-simulator
validation before treating the adapter as operational.

FMS endpoints, runway advisory displays and automatic flight-phase behavior
are not implemented for these aircraft. Do not rely on this
adapter for those features. The current generic runway envelope falls back to
the A320, but runway advisory drawing is disabled for this adapter.

## Offline validation

`scripts/verify-magknight-profiles.py <aircraft-directory> --report <json>`
checks the three actual ACF attachments, panel-textured OBJ vertices, display
UVs and CG-relative click corners without modifying aircraft files. The
2026-11-11 audit found a maximum corner error below 0.00000009 m.

`scripts/test-native.ps1 -MagknightAircraftPath <aircraft-directory>` additionally
loads the built plugin in the mock SDK against those real ACF files. Portable
fixtures cover all three models, the corrected B78X ICAO, off/half/full cycling,
left/right pane selection, range, IRS, brightness, MAP/PLAN, same-ICAO model
switches and invalid-profile rejection. It does not launch or attach to X-Plane.
