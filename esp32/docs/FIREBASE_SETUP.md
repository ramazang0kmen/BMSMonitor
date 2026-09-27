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

## Event log (Cloud Firestore)

State changes are written as history to Firestore (database `(default)`,
`europe-west1`) at `devices/home-battery-01/logs/{epochSeconds}-{type}-{bms}`.
Set `FIREBASE_PROJECT_ID` in `Secrets.h`. Each entry carries `type`,
`severity`, `createdAt`, `bmsIndex` (0 = system), `detail`, a `system`
snapshot (average voltage, SOC, current, power), full `bms1`/`bms2` snapshots
and, for `discharge_stopped`, a `before` snapshot taken while the load was
still on.

| type | when |
| --- | --- |
| `avg_soc_low` / `bms_soc_low` | average / one pack drops to 20 % (re-arms at 25 %) |
| `avg_soc_empty` / `bms_soc_empty` | average / one pack reaches 0 % |
| `avg_soc_full` | average reaches 100 % |
| `discharge_stopped` / `discharge_started` | draw falls below 0.3 A after ≥1 A, and returns |
| `bms_discharge_off/on`, `bms_charge_off/on` | BMS opens / closes a MOSFET |
| `bms_protection` / `_cleared` | JBD protection flags set / cleared |
| `bms_offline` / `bms_online` | pack unreachable for 60 s / back |
| `soc_mismatch` | packs differ by ≥10 % for 30 s |
| `boot` | gateway start, `detail` = ESP reset reason |

Thresholds live in `include/config/LogConfig.h`. Every condition must hold for
a few seconds, and the same condition logs at most once per 5 minutes. Rules
are in `firebase/firestore.rules` (create-only, no update/delete).

## Deploying rules

The rules file can be pasted into the Realtime Database **Rules** editor, or
deployed using a Firebase CLI project configured for `bmsapp-3bcff`. Rule
deployment is intentionally separate from device firmware flashing.
