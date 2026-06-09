# Big AOA Counter

Big AOA Counter is an Axis ACAP that displays AXIS Object Analytics counts as a large on-stream overlay and optional Axis dynamic text value.

## Downloads

Download installable `.eap` packages from the [latest GitHub Release](https://github.com/mrdnlabs/big-aoa-counter/releases/latest):

- `Big_AOA_Counter_<version>_aarch64.eap` for current 64-bit Axis devices
- `Big_AOA_Counter_<version>_armv7hf.eap` for older 32-bit Axis devices

Release assets are built by GitHub Actions from source. The repository does not track generated `.eap` binaries.

## Prerequisites

- Axis camera with ACAP support and AXIS Object Analytics installed
- AXIS OS 12.x tested; currently verified on AXIS OS 12.1.65 and 12.9.57
- Device architecture: `aarch64` or `armv7hf`
- Docker for local builds
- Unsigned ACAP installation enabled when installing unsigned development packages

## How It Works

The ACAP retrieves local VAPIX service credentials over D-Bus, calls the AOA API on loopback, selects a configured countable scenario, and polls the selected count field. The current count is rendered through `axoverlay`; when enabled, it also updates an Axis dynamic text slot with `dynamicoverlay.cgi`. Configuration is stored in ACAP parameters and managed through the app's authenticated reverse-proxy web UI.

## Web UI

Open the app from the Axis applications page. The manifest declares:

```json
"settingPage": "index.html"
```

The setting page redirects to the authenticated reverse-proxy UI:

```text
https://<camera-ip>/local/big_aoa_counter/big-aoa-counter/
```

The standalone widget page is available at:

```text
https://<camera-ip>/local/big_aoa_counter/big-aoa-counter/widget.html
```

The internal CivetWeb server listens on `127.0.0.1:2001` only; clients should not connect to port `2001` directly.

## Configuration

- `Scenario`: select a specific countable AOA scenario, or leave automatic selection enabled
- `Mode=custom`: show only the ACAP's large overlay drawn by this app
- `Mode=dynamic`: update only the selected Axis dynamic text slot, for use with an Axis overlay such as `#D12`
- `Mode=both`: show the large ACAP overlay and update the selected Axis dynamic text slot
- `Dynamic text slot`: slot `1` to `16`, where slot `12` maps to `#D12`
- `AOA field`: choose the count value to display
- `Poll interval (ms)`: bounded from `250` to `10000`; `1000` means one update per second
- `Overlay scale (%)`: scales the large ACAP overlay height and text from `50` to `200`; `100` keeps the default size

Common AOA fields:

- `total`
- `totalVehicle`, computed from `totalCar`, `totalBike`, `totalBus`, `totalTruck`, and `totalOtherVehicle`
- `totalHuman`
- `totalCar`, `totalBike`, `totalBus`, `totalTruck`, `totalOtherVehicle`

## Build

Build a single architecture:

```bash
bash scripts/build.sh aarch64
bash scripts/build.sh armv7hf
```

Packages are copied to `dist/`; the full built app tree is copied to `build/app/`.

GitHub Actions also builds both packages. Manual workflow runs expose them as workflow artifacts, and pushing a tag such as `v0.1.1` creates or updates a GitHub Release with both `.eap` files.

## Install

Install the matching `.eap` file through the Axis device web UI, or use the deployment script with a local `.env.devices` file:

```bash
bash scripts/deploy.sh
```

`.env.devices` is ignored by git and should contain development device credentials only.

If the camera rejects unsigned ACAP uploads, enable unsigned apps:

```bash
curl -sS --anyauth -u root:<password> -X POST \
  "http://<camera-ip>/axis-cgi/applications/config.cgi?action=set&name=AllowUnsigned&value=true"
```

## Verify

Check installed apps:

```bash
curl -sS --anyauth -u root:<password> \
  "http://<camera-ip>/axis-cgi/applications/list.cgi"
```

Check app status through the reverse proxy:

```bash
curl -sS --anyauth -u root:<password> \
  "https://<camera-ip>/local/big_aoa_counter/big-aoa-counter/api/status"
```

Check app logs:

```bash
curl -sS --anyauth -u root:<password> \
  "http://<camera-ip>/axis-cgi/admin/systemlog.cgi?appname=big_aoa_counter"
```

## Tested Devices

- AXIS P3748-PLVE on AXIS OS 12.1.65
- AXIS Q3538-SLVE on AXIS OS 12.9.57

## Known Limitations

- The app expects at least one AOA `crosslinecounting` or `occupancyInArea` scenario unless a specific scenario is configured later.
- Packages are unsigned unless the release process is extended with Axis signing.
- The broad reverse-proxy UI/API route uses admin access because it includes configuration and operational actions.

## Development Notes

Project-specific findings are kept in `C:\_acap\learnings\project-notes\`.

Useful notes:

- `big-aoa-counter-publish-learnings.md`
- `big-aoa-counter-open-button-and-count-source.md`
- `aoa-on-camera-auth-findings.md`
- `p3748-big-aoa-counter-live-notes.md`

## License

MIT License. See [LICENSE](LICENSE).
