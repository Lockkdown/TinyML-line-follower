import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/material.dart';

import '../services/camera_config_service.dart';
import '../services/capture_service.dart';
import '../services/dense_capture_service.dart';
import '../services/robot_service.dart';
import '../widgets/control_button.dart';
import '../widgets/dataset_controls.dart';
import '../widgets/dataset_preview.dart';
import '../widgets/dataset_stats.dart';

class DatasetCaptureScreen extends StatefulWidget {
  final RobotService robotService;
  const DatasetCaptureScreen({super.key, required this.robotService});

  @override
  State<DatasetCaptureScreen> createState() => _DatasetCaptureScreenState();
}

class _DatasetCaptureScreenState extends State<DatasetCaptureScreen> {
  final _classNames = const ['forward', 'left', 'right', 'nothing'];
  final CameraConfigService _cameraConfigService = CameraConfigService();
  late final CaptureService _captureService;
  late final DenseCaptureService _denseCaptureService;
  final Map<String, int> _counts = {'forward': 0, 'left': 0, 'right': 0, 'nothing': 0};
  CameraSettings _cameraSettings = const CameraSettings(quality: 15, resolution: 'QVGA', brightness: 0);
  SessionStats _stats = SessionStats();
  String _selectedClass = 'left';
  int _intervalMs = 400;
  int _countdown = 3;
  bool _countdownActive = false;
  bool _capturing = false;
  bool _paused = false;
  Uint8List? _lastFrame;
  Timer? _countdownTimer;

  @override
  void initState() {
    super.initState();
    _captureService = CaptureService(robotService: widget.robotService);
    _denseCaptureService = DenseCaptureService(
      robotService: widget.robotService,
      captureService: _captureService,
    );
    _initAsync();
  }

  Future<void> _initAsync() async {
    await _captureService.init();
    await _captureService.requestStoragePermission();
    await widget.robotService.setCameraOn();
    await _reloadCounts();
    setState(() {});
  }

  @override
  void dispose() {
    _countdownTimer?.cancel();
    _denseCaptureService.stop();
    _cameraConfigService.dispose();
    widget.robotService.setCameraOff();
    super.dispose();
  }

  Future<void> _reloadCounts() async {
    for (final name in _classNames) {
      _counts[name] = await _captureService.countImages(name);
    }
  }

  Future<void> _applySettings() async {
    final err = await _cameraConfigService.applyCameraSettings(
      _cameraSettings,
      widget.robotService.baseUrl,
    );
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(err == null ? 'Applied ✓' : 'Apply failed: $err'),
      ),
    );
  }

  Future<void> _manualCapture() async {
    final bytes = await widget.robotService.fetchSnapshotBytes(
      timeout: const Duration(milliseconds: 1000),
    );
    if (bytes == null || !mounted) return;
    final saved = await _captureService.saveImage(bytes, _selectedClass);
    if (!saved || !mounted) return;
    setState(() {
      _stats.captured++;
      _counts[_selectedClass] = (_counts[_selectedClass] ?? 0) + 1;
      _lastFrame = bytes;
    });
  }

  void _startCapture() {
    if (_capturing || _countdownActive) return;
    _countdown = 3;
    _countdownActive = true;
    setState(() {});
    _countdownTimer = Timer.periodic(const Duration(seconds: 1), (timer) {
      _countdown--;
      if (_countdown <= 0) {
        timer.cancel();
        _countdownActive = false;
        _capturing = true;
        _paused = false;
        _denseCaptureService.start(_buildConfig());
      }
      if (mounted) setState(() {});
    });
  }

  CaptureConfig _buildConfig() {
    return CaptureConfig(
      className: _selectedClass,
      intervalMs: _intervalMs,
      onFrame: (frame) => setState(() => _lastFrame = frame),
      onSaved: (stats) => setState(() => _stats = stats),
      onSkipped: (reason, stats) {
        if (!mounted) return;
        _stats = stats;
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Skipped: $reason'), duration: const Duration(milliseconds: 500)));
        setState(() {});
      },
      onWifiQuality: (_) {},
      onWifiUnstable: (_) => _handleWifiUnstable(),
    );
  }

  Future<void> _handleWifiUnstable() async {
    if (!_capturing || _paused || !mounted) return;
    _paused = true;
    _denseCaptureService.pause();
    setState(() {});
    final action = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('WiFi unstable'),
        content: const Text('WiFi unstable — 3 failed requests. Reconnect and resume?'),
        actions: [
          TextButton(onPressed: () => Navigator.of(ctx).pop(true), child: const Text('RESUME')),
          TextButton(onPressed: () => Navigator.of(ctx).pop(false), child: const Text('STOP SESSION')),
        ],
      ),
    );
    if (action == true) _togglePause(); else _stopAndSave();
  }

  void _togglePause() {
    if (!_capturing) return;
    _paused = !_paused;
    if (_paused) {
      _denseCaptureService.pause();
    } else {
      _denseCaptureService.start(_buildConfig());
    }
    setState(() {});
  }

  Future<void> _stopAndSave() async {
    _capturing = false;
    _paused = false;
    _stats = _denseCaptureService.stop();
    await _reloadCounts();
    if (!mounted) return;
    setState(() {});
    final more = await showSessionSummaryDialog(context, _stats, _counts);
    if (!more && mounted) Navigator.of(context).pop();
  }

  void _sendCommand(String cmd) => widget.robotService.sendCommand(cmd);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Dataset Capture')),
      body: Padding(
        padding: const EdgeInsets.all(10),
        child: Column(
          children: [
            Expanded(
              flex: 4,
              child: DatasetPreview(
                frame: _lastFrame,
                captured: _stats.captured,
                skipped: _stats.skippedTotal,
                className: _selectedClass,
                classCount: _counts[_selectedClass] ?? 0,
                targetPerClass: targetPerClass,
              ),
            ),
            const SizedBox(height: 8),
            DatasetStatsBar(stats: _stats),
            const SizedBox(height: 8),
            Expanded(
              flex: 5,
              child: DatasetControls(
                selectedClass: _selectedClass,
                counts: _counts,
                settings: _cameraSettings,
                intervalMs: _intervalMs,
                capturing: _capturing,
                paused: _paused,
                countdownActive: _countdownActive,
                countdownValue: _countdown,
                onClassSelected: (name) => setState(() => _selectedClass = name),
                onSettingsChanged: (settings) => setState(() => _cameraSettings = settings),
                onApplySettings: _applySettings,
                onIntervalChanged: (value) => setState(() => _intervalMs = value),
                onStartCapture: _startCapture,
                onPauseCapture: _togglePause,
                onStopAndSave: _stopAndSave,
                onCancelCountdown: () => setState(() => _countdownActive = false),
                onManualCapture: _manualCapture,
              ),
            ),
            const SizedBox(height: 8),
            _buildDrivingControls(),
          ],
        ),
      ),
    );
  }

  Widget _buildDrivingControls() {
    return Column(
      children: [
        ControlButton(backgroundColor: Colors.blue, onPressed: () => _sendCommand('forward'), onReleased: () => _sendCommand('stop'), onCanceled: () => _sendCommand('stop'), child: const Icon(Icons.arrow_upward, color: Colors.white)),
        const SizedBox(height: 8),
        Row(mainAxisAlignment: MainAxisAlignment.spaceEvenly, children: [
          ControlButton(backgroundColor: Colors.green, onPressed: () => _sendCommand('left'), onReleased: () => _sendCommand('stop'), onCanceled: () => _sendCommand('stop'), child: const Icon(Icons.arrow_back, color: Colors.white)),
          ControlButton(backgroundColor: Colors.red, onPressed: () => _sendCommand('stop'), child: const Icon(Icons.stop, color: Colors.white)),
          ControlButton(backgroundColor: Colors.green, onPressed: () => _sendCommand('right'), onReleased: () => _sendCommand('stop'), onCanceled: () => _sendCommand('stop'), child: const Icon(Icons.arrow_forward, color: Colors.white)),
        ]),
        const SizedBox(height: 8),
        ControlButton(backgroundColor: Colors.orange, onPressed: () => _sendCommand('backward'), onReleased: () => _sendCommand('stop'), onCanceled: () => _sendCommand('stop'), child: const Icon(Icons.arrow_downward, color: Colors.white)),
      ],
    );
  }
}
