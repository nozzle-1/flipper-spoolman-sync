# flipper-spoolman-sync
Sync your Bambu Lab spools to Spoolman using Flipper Zero

## Configuration

The Spoolman base URL is now loaded from:

`/ext/apps_data/spoolman_sync/spoolman_url.txt`

On first launch, the app creates this file automatically with the default value:

`https://example.com`

To point the app to another Spoolman instance, edit that file on the Flipper and put the full base URL on the first line, for example:

`https://spoolman.example.com`

You can also change it directly from the app:

- Open `Server URL` from the main menu
- Use the on-screen keyboard with URL symbols and number mode
- Press `OK` on `DONE` to test `/api/v1/health` and save only if Spoolman replies correctly
- Press `BACK` to delete
- Press `BACK` long to cancel and return to the menu
