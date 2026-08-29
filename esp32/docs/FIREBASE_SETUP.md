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
4. Assign `gateway: true` to the device account and `viewer: true` to mobile
   users from a trusted Admin SDK environment, then apply the claim-based rules
   from `firebase/database.rules.json`. Do not enable public read or write access.
5. Keep the real Wi-Fi and Firebase credentials only in the ignored
   `include/config/Secrets.h` file. Copy `Secrets.example.h` when setting up a
   new checkout.

## Runtime paths

- Status heartbeat: `/devices/home-battery-01/status`
- Remote session request: `/devices/home-battery-01/control/remoteSession/requestedAt`
- Conditional live snapshot: `/devices/home-battery-01/live`

The status heartbeat is written every 30 seconds. The mobile app writes one
Firebase server timestamp to `control/remoteSession/requestedAt` when the
dashboard opens, when the app resumes, or when the user requests a manual
refresh. Live telemetry is sent for 60 seconds measured from that server
timestamp. The request is not renewed periodically, and historical snapshots
are neither queued nor written to flash.

## Deploying rules

The rules file can be pasted into the Realtime Database **Rules** editor, or
deployed using a Firebase CLI project configured for `bmsapp-3bcff`. Rule
deployment is intentionally separate from device firmware flashing.
