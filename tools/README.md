# Tools

This directory contains helper scripts used to prepare and import Bambu Lab filament definitions into Spoolman.

## Goal

The main purpose of these scripts is not just to import a vendor catalog.

The real goal is to create Spoolman filament entries that include the internal Bambu Lab filament identifier from the RFID tags as `article_number` in Spoolman.

That matters because the Flipper app can read those Bambu identifiers directly from the spool tags. Once the same identifier is stored in Spoolman, the app can find the correct filament immediately without maintaining a custom translation layer or relying on fuzzy name matching.

This is important because the filament names available in SpoolmanDB do not always match the real product names used by Bambu Studio or printed on physical Bambu Lab spools.

## Scripts

### `generate_bambu_lab_filaments_file.js`

This script extracts filament metadata from Bambu Studio profiles and builds a normalized JSON file.

The generated file contains data such as:

- Bambu internal filament ID
- material
- color name
- color codes
- vendor
- density
- default spool and filament weights
- extrusion temperature when available

Its output is intended to be consumed by the import script.

### `import_bambu_lab_filaments_to_spoolman.js`

This script reads the generated JSON file and creates missing filament entries in Spoolman.

During import, the Bambu internal filament ID is written into Spoolman as:

`article_number`

This makes Bambu-tag-based lookup straightforward in the Flipper app:

- scan tag
- read Bambu filament ID
- search Spoolman by `article_number`
- retrieve the matching filament directly

## Why this approach exists

Without these scripts, filament matching would be much less reliable because:

- SpoolmanDB naming does not exactly match Bambu Studio naming
- some Bambu products have naming variations depending on profile source or locale
- matching by color and material alone is too ambiguous

Using the Bambu internal ID as `article_number` gives a stable, machine-readable link between:

- the physical RFID tag
- Bambu Studio profile data
- the Spoolman filament record

## Typical flow

1. Generate the intermediate JSON catalog from Bambu Studio data.
2. Import that catalog into Spoolman.
3. Store each Bambu filament code in `article_number`.
4. Let the Flipper app use that identifier for direct lookup.

## Make targets

The repository provides convenience targets in the root `Makefile`:

```sh
make fetch_filaments
make import_filaments SPOOLMAN_URL=http://localhost:7912
make sync_filaments SPOOLMAN_URL=http://localhost:7912
```

Optional variables:

- `FILAMENTS_FILE` to override the generated JSON path
- `SPOOLMAN_VENDOR` to override the vendor name used during import

## Notes

- These scripts are focused on Bambu Lab filaments.
- They are meant to support the Flipper-to-Spoolman workflow in this repository.
- The generated `tmp/` files are intermediate artifacts and can be regenerated.
