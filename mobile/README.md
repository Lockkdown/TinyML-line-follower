# Robot Controller Flutter App

Native Android controller for the ESP32-S3 line-following robot over WiFi.

## Features

- Live camera feed from `/snapshot` endpoint (polls every 250ms)
- Hold-to-move controls: Forward, Left, Right, Backward, Stop
- Connection status badge (Connected/Disconnected)
- Configurable robot IP address in Settings
- Dark theme with large touch targets

## Setup

1. Install Flutter SDK (>=3.0.0)
2. Run `flutter pub get` in this directory
3. Connect an Android device or start an emulator
4. Run `flutter run`

## ESP32 Endpoints Required

- `GET /` — health check (returns 200)
- `GET /snapshot` — JPEG camera frame
- `POST /forward`
- `POST /left`
- `POST /right`
- `POST /backward`
- `POST /stop`

## Usage

1. Open the app
2. Tap the settings icon to set your robot's IP address (default: `http://192.168.43.200`)
3. Hold a movement button to drive; release to stop
4. The center button is always Stop; the bottom button is Backward

## File Structure

```
lib/
├── main.dart                    — app entry point
├── screens/
│   ├── controller_screen.dart   — main UI (camera + buttons)
│   └── settings_screen.dart     — IP address config
├── services/
│   └── robot_service.dart       — HTTP calls to ESP32
└── widgets/
    ├── camera_view.dart         — camera polling widget
    └── control_button.dart     — reusable hold button
```

## Dependencies

- `http: ^1.2.0` — network requests
- `shared_preferences: ^2.2.0` — persist IP address
