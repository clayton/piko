# Piko

Pocket instruments for the Waveshare ESP32-S3 Touch AMOLED 1.8 V2.

## Unified firmware

`piko/` combines Radar, Internet Geiger, and Raises Piko in one binary. Hold the **top button for about 900 ms** to open the app switcher. In the switcher, a short top press cycles apps and a short bottom press launches the highlighted one. Short top and bottom presses keep each app's normal behavior while you are inside an app. Hold the bottom button for about six seconds to power off.

The unified firmware starts in Raises Piko on boot. After 45 seconds without movement or a button press, the display dims to a tiny status line. Pick Piko up, move it, or press either button to restore the full app. The first button press only wakes the display.

```sh
make setup
cp piko/config.h.example piko/config.h
make piko-flash
```

Standalone sketches below still build and flash independently.

## Internet Geiger counter

`internet_geiger/` passively samples nearby Wi-Fi management and data frames. It turns radio traffic into a rolling counts-per-minute score, a 60-second activity graph, pixel flashes, and speaker clicks.

```sh
make setup
make geiger-flash
```

The top button cycles LOW, MED, and HIGH sensitivity. The bottom button mutes the speaker. Hold the bottom button for about six seconds to power off. The firmware does not join a network, save packet contents, or transmit captured data.

## Raises Piko

`raises_piko/` turns Piko into a tiny incident creature. A signed Raises webhook changes his mood when a notice, new error, regression, or GitHub issue arrives. See [`raises_piko/README.md`](raises_piko/README.md) for the Cloudflare Worker bridge and flashing steps.

## Aircraft display

Piko reads a local dump1090-compatible feed and shows one aircraft at a time with ADSBDB route and airframe details. The top button cycles **NEAR**, **HIGH**, and **BIG** rankings. The bottom button advances to the next aircraft. Hold the bottom button for about six seconds to power off.

## Hardware

- Waveshare ESP32-S3 Touch AMOLED 1.8 V2
- Local dump1090 or FlightRadar24 feeder
- Wi-Fi access to the feeder

## Configure

Copy the public example and enter your local values:

```sh
cp piko/config.h.example piko/config.h
# or for standalone radar only:
cp radar/config.h.example radar/config.h
```

Unified `piko/config.h` contains Wi-Fi credentials, dump1090 feeder addresses, receiver coordinates, and Raises bridge settings. Standalone `radar/config.h` and `raises_piko/config.h` remain for individual sketches. All config files are ignored by Git.

The Makefile can copy any ignored local template into place. Set it in an ignored `Makefile.local`:

```make
CONFIG_TEMPLATE = piko/config.h.tpl
CONFIG_DEST = piko/config.h
```

Then run `make secrets`. This supports a 1Password `op inject` workflow outside the public repository.

## Build and flash

```sh
brew install arduino-cli
make setup
make piko-flash    # all three apps
make flash         # radar only
make geiger-flash  # geiger only
make raises-flash  # raises only
make clean-secrets
```

The default serial port is `/dev/cu.usbmodem1101`. Override it when needed:

```sh
make flash PORT=/dev/cu.usbmodemXXXX
```

## Aircraft enrichment proxy

The firmware expects `PLANE_DETAILS_URL` to return the ADSBDB aircraft response for query parameters named `hex` and `callsign`. A minimal Lighttpd CGI proxy can forward:

```text
https://api.adsbdb.com/v0/aircraft/{hex}?callsign={callsign}
```

Keep the proxy on the feeder so the ESP32 only makes local HTTP requests.

## Factory backup

Private device backups belong under the ignored `backups/` directory. To enable `make restore`, copy `Makefile.local.example` to `Makefile.local` and set `FACTORY_BACKUP` to your image path.
