# Firebase setup

## Project

- Project ID: `bmsapp-3bcff`
- Realtime Database: `https://bmsapp-3bcff-default-rtdb.europe-west1.firebasedatabase.app/`
- Device ID: `home-battery-01`
- Device root: `/devices/home-battery-01`

## Firebase Console checklist

1. Open **Build > Realtime Database** and confirm the database is enabled in
   `europe-west1`.
2. Open **Build > Authentication > Sign-in method** and enable
   **Email/Password**.
3. Under **Authentication > Users**, confirm that a dedicated Firebase
   Authentication device user exists.
4. Apply the authenticated-only rules from
   `firebase/database.rules.json`. Do not enable public read or write access.
5. Keep the real Wi-Fi and Firebase credentials only in the ignored
   `include/config/Secrets.h` file. Copy `Secrets.example.h` when setting up a
   new checkout.

## Runtime paths

- Status heartbeat: `/devices/home-battery-01/status`
- Viewer heartbeat: `/devices/home-battery-01/control/viewer/lastSeen`
- Conditional live snapshot: `/devices/home-battery-01/live`

The status heartbeat is written every 30 seconds. A viewer must update
`control/viewer/lastSeen` with a Firebase server timestamp. Live telemetry is
sent only while that value is no more than 15 seconds old. Historical
snapshots are neither queued nor written to flash.

## Deploying rules

The rules file can be pasted into the Realtime Database **Rules** editor, or
deployed using a Firebase CLI project configured for `bmsapp-3bcff`. Rule
deployment is intentionally separate from device firmware flashing.
