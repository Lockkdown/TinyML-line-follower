import 'dart:async';
import 'dart:typed_data';

import 'package:image/image.dart' as img;

import 'capture_service.dart';
import 'robot_service.dart';

class SessionStats {
  int captured = 0;
  int skippedFrozen = 0;
  int skippedDark = 0;
  int skippedBright = 0;
  int wifiErrors = 0;

  int get skippedTotal => skippedFrozen + skippedDark + skippedBright;
}

class CaptureConfig {
  final String className;
  final int intervalMs;
  final void Function(Uint8List frame) onFrame;
  final void Function(SessionStats stats) onSaved;
  final void Function(String reason, SessionStats stats) onSkipped;
  final void Function(double successRate) onWifiQuality;
  final void Function(SessionStats stats) onWifiUnstable;

  const CaptureConfig({
    required this.className,
    required this.intervalMs,
    required this.onFrame,
    required this.onSaved,
    required this.onSkipped,
    required this.onWifiQuality,
    required this.onWifiUnstable,
  });
}

class DenseCaptureService {
  final RobotService robotService;
  final CaptureService captureService;
  final SessionStats _stats = SessionStats();
  final List<bool> _recentRequests = <bool>[];
  Timer? _timer;
  CaptureConfig? _config;
  Uint8List? _previousFrame;
  bool _busy = false;
  int _consecutiveErrors = 0;

  DenseCaptureService({
    required this.robotService,
    required this.captureService,
  });

  double get wifiSuccessRate {
    if (_recentRequests.isEmpty) return 1;
    final ok = _recentRequests.where((value) => value).length;
    return ok / _recentRequests.length;
  }

  bool get isRunning => _timer?.isActive ?? false;

  void start(CaptureConfig config) {
    _config = config;
    _timer?.cancel();
    _timer = Timer.periodic(
      Duration(milliseconds: config.intervalMs),
      (_) => _tick(),
    );
  }

  void pause() {
    _timer?.cancel();
  }

  void resume() {
    final cfg = _config;
    if (cfg == null) return;
    start(cfg);
  }

  void resetStats() {
    _stats.captured = 0;
    _stats.skippedFrozen = 0;
    _stats.skippedDark = 0;
    _stats.skippedBright = 0;
    _stats.wifiErrors = 0;
    _consecutiveErrors = 0;
    _previousFrame = null;
  }

  SessionStats stop() {
    _timer?.cancel();
    _previousFrame = null;
    _consecutiveErrors = 0;
    return _stats;
  }

  Future<void> _tick() async {
    final cfg = _config;
    if (cfg == null || _busy) return;
    _busy = true;
    var bytes = await robotService.fetchSnapshotBytes();
    if (bytes == null) {
      await Future<void>.delayed(const Duration(milliseconds: 120));
      bytes = await robotService.fetchSnapshotBytes();
    }
    if (bytes == null) {
      _stats.wifiErrors++;
      _consecutiveErrors++;
      _trackRequest(false, cfg);
      if (_consecutiveErrors >= 3) cfg.onWifiUnstable(_stats);
      _busy = false;
      return;
    }
    _consecutiveErrors = 0;
    _trackRequest(true, cfg);
    final brightnessReason = _checkBrightness(bytes);
    if (brightnessReason != null) {
      if (brightnessReason == 'dark') _stats.skippedDark++;
      if (brightnessReason == 'bright') _stats.skippedBright++;
      cfg.onSkipped(brightnessReason, _stats);
      _previousFrame = bytes;
      _busy = false;
      return;
    }
    if (_isFrozen(bytes, _previousFrame)) {
      _stats.skippedFrozen++;
      cfg.onSkipped('frozen', _stats);
      _previousFrame = bytes;
      _busy = false;
      return;
    }
    final saved = await captureService.saveImage(bytes, cfg.className);
    if (saved) {
      _stats.captured++;
      cfg.onFrame(bytes);
      cfg.onSaved(_stats);
    } else {
      _stats.wifiErrors++;
      cfg.onSkipped('wifi', _stats);
    }
    _previousFrame = bytes;
    _busy = false;
  }

  void _trackRequest(bool success, CaptureConfig cfg) {
    _recentRequests.add(success);
    if (_recentRequests.length > 10) _recentRequests.removeAt(0);
    cfg.onWifiQuality(wifiSuccessRate);
  }

  String? _checkBrightness(Uint8List bytes) {
    final image = img.decodeJpg(bytes);
    if (image == null) return 'dark';
    final grayscale = img.grayscale(image);
    final pixels = grayscale.getBytes();
    int total = 0;
    for (int i = 0; i < pixels.length; i += 4) {
      total += pixels[i];
    }
    final mean = total / (pixels.length / 4);
    if (mean < 20) return 'dark';
    if (mean > 235) return 'bright';
    return null;
  }

  bool _isFrozen(Uint8List current, Uint8List? previous) {
    if (previous == null) return false;
    final now = img.copyResize(img.decodeJpg(current)!, width: 32, height: 32);
    final old = img.copyResize(img.decodeJpg(previous)!, width: 32, height: 32);
    final nowBytes = img.grayscale(now).getBytes();
    final oldBytes = img.grayscale(old).getBytes();
    int diff = 0;
    for (int i = 0; i < nowBytes.length; i += 4) {
      diff += (nowBytes[i] - oldBytes[i]).abs();
    }
    final mad = diff / (nowBytes.length / 4);
    return mad < 1.5;
  }
}
