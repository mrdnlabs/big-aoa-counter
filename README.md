# Big AOA Counter

`Big AOA Counter` is an ACAP for Axis cameras that provides a much larger, easier-to-read overlay for AXIS Object Analytics counts than the standard OSD counter.

It supports:

- a custom on-stream overlay rendered by the ACAP
- dynamic text slot updates for `#D1` to `#D16`
- persisted ACAP configuration through app parameters
- an on-camera web UI for status/config work
- a standalone widget page for VMS/dashboard embedding

## Configuration

The web UI now discovers supported AOA scenarios and shows them in a drop-down.

The app now declares a proper settings page, so Axis should show an `Open` entry/button in the apps view. That entry redirects into the live UI served by the ACAP on port `2001`.

You can also open the UI directly:

- `http://<camera-ip>:2001/`
- `http://<camera-ip>:2001/widget.html` for the widget view

The generated Axis settings form is not the main operator UI. The custom UI is where scenario selection is presented as a live drop-down.

- `Scenario`: select a specific countable AOA scenario, or leave it on automatic selection
- `Mode=custom`: only the ACAP's large overlay is shown
- `Mode=dynamic`: only the selected dynamic text slot is updated
- `Mode=both`: both display paths are updated
- `AOA field`: choose the AOA count field from a drop-down with a short description for each option

The displayed count is intended to mirror the selected AOA scenario value. Legacy manual override/test controls were removed so the UI no longer presents a count that can diverge from AOA.

The web UI auto-refreshes live status but no longer overwrites config fields while you are editing them.

Typical `AOA field` values:

- `total` for the overall count
- `totalVehicle` for the summed vehicle count across `totalCar`, `totalBike`, `totalBus`, `totalTruck`, and `totalOtherVehicle`
- `totalHuman` for human counts from class-specific scenarios
- `totalCar` for vehicle counts from class-specific scenarios
- `totalBike`, `totalBus`, `totalTruck`, and `totalOtherVehicle` for the other common AOA vehicle classes

`Category=total` in the earlier UI meant exactly this AOA response field. The label is now `AOA field` to make that clearer.

The custom overlay uses the configured `Label`, not the raw AOA field name.

## Current status

- Latest built package: `build/app/Big_AOA_Counter_0_1_0_aarch64.eap`
- Latest deployed version on tested cameras: `0.1.0`
- Verified running fully on-camera on:
  - `AXIS P3748-PLVE` on AXIS OS `12.1.65`
  - `AXIS Q3538-SLVE` on AXIS OS `12.9.57`

## Important implementation note

This ACAP updates dynamic text slots on-camera using:

- `/axis-cgi/dynamicoverlay.cgi?action=settext&text_index=<n>&text=<value>`

The larger custom overlay path is the primary display path.

For the tested cameras, the count path is now fully on-camera:

- the ACAP retrieves VAPIX runtime credentials over D-Bus
- the ACAP calls AOA locally over loopback
- the ACAP writes dynamic text directly on-camera
- no host-side polling bridge is required
- the published app does not store external device credentials in ACAP parameters

## Files

- Main ACAP source: [app/big_aoa_counter.c](/mnt/c/big-aoa-counter/app/big_aoa_counter.c)
- Manifest: [app/manifest.json](/mnt/c/big-aoa-counter/app/manifest.json)
- Build container: [Dockerfile](/mnt/c/big-aoa-counter/Dockerfile)
- Build script: [scripts/build.sh](/mnt/c/big-aoa-counter/scripts/build.sh)
- Deploy script: [scripts/deploy.sh](/mnt/c/big-aoa-counter/scripts/deploy.sh)
- Main UI: [app/html/index.html](/mnt/c/big-aoa-counter/app/html/index.html)
- Widget UI: [app/html/widget.html](/mnt/c/big-aoa-counter/app/html/widget.html)
- Device credentials file: [`.env.devices`](/mnt/c/big-aoa-counter/.env.devices)

## Build

```bash
./scripts/build.sh
```

Artifacts are copied to:

```bash
build/app/
```

## Deploy

Update [`.env.devices`](/mnt/c/big-aoa-counter/.env.devices), then:

```bash
./scripts/deploy.sh
```

This script:

- uploads the `.eap`
- starts the ACAP

## Enable unsigned apps

If the camera rejects unsigned ACAP uploads, enable them first:

```bash
curl -sS --anyauth -u root:<password> -X POST \
  "http://<camera-ip>/axis-cgi/applications/config.cgi?action=set&name=AllowUnsigned&value=true"
```

Reference note:

- [enable-unsigned-apps.md](/mnt/c/_acap/enable-unsigned-apps.md)

## Verify deployment

Check installed apps:

```bash
curl -sS --anyauth -u root:<password> \
  "http://<camera-ip>/axis-cgi/applications/list.cgi"
```

Check app log:

```bash
curl -sS --anyauth -u root:<password> \
  "http://<camera-ip>/axis-cgi/admin/systemlog.cgi?appname=big_aoa_counter"
```

## Dynamic text slots

Dynamic text indices map like this:

- `text_index=1` maps to `#D1`
- `text_index=12` maps to `#D12`

Example:

```bash
curl --digest -u root:<password> \
  "http://<camera-ip>/axis-cgi/dynamicoverlay.cgi?action=settext&text_index=12&text=CountLine%2042"
```

Read back:

```bash
curl --digest -u root:<password> \
  "http://<camera-ip>/axis-cgi/dynamicoverlay.cgi?action=gettext&text_index=12"
```

## Research notes

Project notes were saved here:

- [axis-acap-12.9-big-aoa-counter-notes.md](/mnt/c/_acap/axis-acap-12.9-big-aoa-counter-notes.md)
- [p3748-big-aoa-counter-live-notes.md](/mnt/c/_acap/p3748-big-aoa-counter-live-notes.md)
- [enable-unsigned-apps.md](/mnt/c/_acap/enable-unsigned-apps.md)

## Is further work required?

For the current deployed test state, no required work is blocking:

- the ACAP builds
- the ACAP deploys
- the ACAP runs on the camera
- the large overlay path is implemented
- the dynamic text update path is implemented

Optional further work:

- reduce startup/log verbosity and tighten runtime logging
- tune overlay layout, font sizing, and placement per stream/view area
- add explicit install-time creation of a supported counting scenario instead of relying on post-deploy configuration
