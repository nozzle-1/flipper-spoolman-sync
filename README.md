# Flipper Spoolman Sync

Import Bambu Lab spools into Spoolman by scanning them with a Flipper Zero.

This project is a Flipper Zero app built for a simple use case: take a Bambu Lab spool, scan its RFID tag, and create or match the corresponding spool in Spoolman without typing references, colors, or material details by hand.

It is especially useful if you already manage your filament stock in Spoolman and want a faster way to register physical spools as they arrive on your shelf.

## Why this exists

Bambu Lab spools already contain useful data in their RFID tags. This app reads that data from a Flipper Zero, extracts the information needed to identify the filament, and uses the Spoolman API to:

- create a new spool directly from the scan
- find an existing spool linked to the scanned tag
- assign tags to existing untagged spools in bulk
- inspect the raw decoded data from a Bambu tag

The result is a much smoother onboarding flow for Bambu Lab filament inside a Spoolman-based setup.

## Requirements

To use this app, you need:

- a Flipper Zero with NFC support
- a Wi-Fi development board connected to the Flipper
- [FlipperHTTP](https://github.com/jblanked/FlipperHTTP) flashed on the Wi-Fi module
- `FlipperHTTP` version `2.1.8` or newer
- a reachable Spoolman instance

`FlipperHTTP` is required because the app communicates with Spoolman over HTTP through the Wi-Fi module.

## What the app does

### 1. Create a spool from a scan

Scan a Bambu Lab spool and the app will:

- read the RFID tag
- extract the Bambu filament identifiers
- look up the matching filament in Spoolman
- create a new spool in Spoolman
- write the scanned RFID value into `extra.tag`

This is the main workflow and the reason the project exists.

### 2. Find a spool from a scan

If a spool has already been registered, you can scan it again to retrieve the linked Spoolman spool.

The app shows:

- spool ID
- material
- filament name
- remaining weight when available

This is useful to quickly identify a spool already stored in Spoolman.

### 3. Tag existing spools

If you already have spools in Spoolman but some of them were created without RFID data, the app can help you add tags afterward.

It loads the list of spools, highlights the ones missing a tag, then lets you:

- browse untagged spools
- scan a physical Bambu spool
- save the RFID value into the selected Spoolman spool

This is a practical migration path if your Spoolman library already exists.

### 4. Read raw spool tag data

There is also a raw scan mode for reading and inspecting decoded information from the Bambu RFID tag, including data such as:

- filament type
- color values
- spool weight
- diameter
- temperature-related values
- production date

This mode is useful for debugging, validation, and understanding what the tag contains.

## Spoolman integration

The app stores the scanned RFID identifier in the Spoolman spool field:

`extra.tag`

That makes it a very good fit with [SpoolmanSync](https://github.com/gibz104/SpoolmanSync), which also relies on `extra.tag`.

Used together, the workflow becomes very convenient:

- use this app to register Bambu Lab spools in Spoolman by scanning them
- let SpoolmanSync detect the active filament automatically
- let SpoolmanSync handle usage tracking and spool consumption

In short, this app helps populate `extra.tag`, and SpoolmanSync can then use that tag for automatic spool detection and material accounting.

## Configuration

The Spoolman base URL is stored on the Flipper at:

`/ext/apps_data/spoolman_sync/spoolman_url.txt`

If the file does not exist yet, or is empty, the app opens directly on the server URL screen.

You can also configure it from inside the app:

- open `Server settings` from the main menu
- enter the full base URL of your Spoolman instance
- confirm with `DONE`
- the app tests `/api/v1/health`
- the URL is saved only if Spoolman replies successfully

Example:

```text
https://spoolman.example.com
```

## Typical workflow

1. Flash `FlipperHTTP` `2.1.8+` to the Wi-Fi module.
2. Start the app on the Flipper Zero.
3. Configure your Spoolman server URL.
4. Choose `Create a spool`.
5. Scan a Bambu Lab spool.
6. Confirm creation.
7. The spool is created in Spoolman with its RFID tag stored in `extra.tag`.

After that, tools such as SpoolmanSync can reuse that tag for automatic identification.

## Build

This project uses `ufbt`.

```sh
make build
```

For development:

```sh
make dev
```

## Acknowledgements

The following projects were especially helpful while building this app:

- [Bambu-Research-Group/RFID-Tag-Guide](https://github.com/Bambu-Research-Group/RFID-Tag-Guide) for documenting and explaining Bambu Lab RFID tag data
- [Donkie/Spoolman](https://github.com/Donkie/Spoolman) for the spool management platform and API this app integrates with
- [gibz104/SpoolmanSync](https://github.com/gibz104/SpoolmanSync) for the `extra.tag`-based automation workflow that pairs well with this project

## Current scope

This app is focused on Bambu Lab spools and Spoolman integration through RFID scanning. It is designed to reduce manual entry, speed up spool registration, and make downstream automation with tools like SpoolmanSync much easier.
