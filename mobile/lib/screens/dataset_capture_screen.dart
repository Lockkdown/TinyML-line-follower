import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/material.dart';

import '../services/camera_config_service.dart';
import '../services/capture_service.dart';
import '../services/dense_capture_service.dart';
import '../services/robot_service.dart';
import '../services/transfer_service.dart';
import '../widgets/dataset_controls.dart';
import '../widgets/dataset_preview.dart';
import '../widgets/dataset_stats.dart';

// QQVGA keeps JPEG small for WiFi; training resizes to 96x96 anyway.
const _datasetCameraSettings = CameraSettings(
  quality: 15,
  resolution: 'QQVGA',
  brightness: 0,
);

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
  SessionStats _stats = SessionStats();
  String _selectedClass = 'left';
  static const int _captureIntervalMs = 80;
  int _countdown = 3;
  bool _countdownActive = false;
  bool _capturing = false;
  bool _paused = false;
  Uint8List? _lastFrame;
  Timer? _countdownTimer;
  Timer? _previewTimer;
  bool _robotCameraBusy = false;
  bool _robotCameraOn = false;
  bool _previewBusy = false;
  final TransferService _transferService = TransferService();
  bool _transferServerRunning = false;
  List<String> _transferIps = [];

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
    await _reloadCounts();
    setState(() {});
  }

  void _stopPreviewPolling() {
    _previewTimer?.cancel();
    _previewTimer = null;
  }

  void _startPreviewPolling() {
    _stopPreviewPolling();
    _previewTimer = Timer.periodic(const Duration(milliseconds: 200), (_) {
      _runPreviewTick();
    });
  }

  Future<void> _runPreviewTick() async {
    if (!mounted || _capturing || _previewBusy) return;
    _previewBusy = true;
    try {
      final bytes = await widget.robotService.fetchSnapshotBytes();
      if (!mounted || _capturing || bytes == null) return;
      setState(() => _lastFrame = bytes);
    } finally {
      _previewBusy = false;
    }
  }

  Future<void> _turnOnRobotCamera() async {
    if (_robotCameraBusy) return;
    setState(() => _robotCameraBusy = true);
    final ok = await widget.robotService.setCameraOn();
    if (!mounted) return;
    setState(() => _robotCameraBusy = false);
    if (!ok) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Không bật được camera trên robot')),
      );
      return;
    }
    setState(() => _robotCameraOn = true);
    _cameraConfigService.applyCameraSettings(
      _datasetCameraSettings,
      widget.robotService.baseUrl,
    );
    _startPreviewPolling();
  }

  @override
  void dispose() {
    _countdownTimer?.cancel();
    _stopPreviewPolling();
    _denseCaptureService.stop();
    _cameraConfigService.dispose();
    _transferService.stop();
    widget.robotService.setCameraOff();
    super.dispose();
  }

  Future<void> _toggleTransferServer() async {
    if (_transferServerRunning) {
      await _transferService.stop();
      if (!mounted) return;
      setState(() => _transferServerRunning = false);
      return;
    }
    final ips = await _transferService.getAllLanIps();
    await _transferService.start();
    if (!mounted) return;
    setState(() {
      _transferServerRunning = true;
      _transferIps = ips;
    });
    _showTransferDialog();
  }

  void _showTransferDialog() {
    final ips = _transferIps.isNotEmpty ? _transferIps : ['?.?.?.?'];
    showDialog<void>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF1E1E1E),
        title: const Text('WiFi Transfer', style: TextStyle(color: Colors.white)),
        content: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text(
                'Mở link dưới đây trên trình duyệt máy tính\n(thử từng cái nếu không kết nối được):',
                style: TextStyle(color: Colors.grey, fontSize: 13),
              ),
              const SizedBox(height: 12),
              ...ips.map((ip) => Padding(
                padding: const EdgeInsets.only(bottom: 6),
                child: SelectableText(
                  'http://$ip:$transferServerPort',
                  style: const TextStyle(
                    color: Colors.lightBlueAccent,
                    fontSize: 15,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              )),
              const SizedBox(height: 12),
              const Text(
                'Bấm vào class để tải ZIP. Server chạy đến khi bấm icon WiFi lần nữa.',
                style: TextStyle(color: Colors.grey, fontSize: 12),
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: const Text('OK'),
          ),
        ],
      ),
    );
  }

  Future<void> _reloadCounts() async {
    for (final name in _classNames) {
      _counts[name] = await _captureService.countImages(name);
    }
  }

  Future<void> _manualCapture() async {
    final bytes = await widget.robotService.fetchSnapshotBytes();
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
    // Stop preview immediately so no in-flight requests overlap with dense capture.
    _stopPreviewPolling();
    _denseCaptureService.resetStats();
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
      intervalMs: _captureIntervalMs,
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
    if (action == true) {
      _togglePause();
    } else {
      _stopAndSave();
    }
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
    if (_robotCameraOn) {
      _startPreviewPolling();
    }
    setState(() {});
    final more = await showSessionSummaryDialog(context, _stats, _counts);
    if (!more && mounted) Navigator.of(context).pop();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Dataset Capture'),
        actions: [
          IconButton(
            icon: Icon(
              _transferServerRunning ? Icons.wifi : Icons.wifi_off,
              color: _transferServerRunning ? Colors.greenAccent : null,
            ),
            tooltip: _transferServerRunning ? 'Dừng server / Xem link tải' : 'Bật server tải dataset về máy',
            onPressed: _transferServerRunning
                ? () async {
                    _showTransferDialog();
                  }
                : _toggleTransferServer,
          ),
          IconButton(
            icon: Icon(_robotCameraOn ? Icons.videocam : Icons.videocam_off_outlined),
            tooltip: 'Bật camera robot (chuẩn bị trước khi thu)',
            onPressed: _robotCameraBusy ? null : _turnOnRobotCamera,
          ),
          IconButton(
            icon: const Icon(Icons.refresh),
            tooltip: 'Reload image counts',
            onPressed: () async {
              await _reloadCounts();
              if (!mounted) return;
              setState(() {});
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text('Đã cập nhật số lượng ảnh'),
                  duration: Duration(seconds: 1),
                ),
              );
            },
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(10),
        child: Column(
          children: [
            Expanded(
              flex: 7,
              child: DatasetPreview(
                frame: _lastFrame,
                captured: _stats.captured,
                skipped: _stats.skippedTotal,
                className: _selectedClass,
                classCount: _counts[_selectedClass] ?? 0,
                targetPerClass: targetPerClass,
              ),
            ),
            const SizedBox(height: 6),
            DatasetStatsBar(stats: _stats),
            const SizedBox(height: 6),
            Expanded(
              flex: 3,
              child: DatasetControls(
                selectedClass: _selectedClass,
                counts: _counts,
                capturing: _capturing,
                paused: _paused,
                countdownActive: _countdownActive,
                countdownValue: _countdown,
                onClassSelected: (name) => setState(() => _selectedClass = name),
                onStartCapture: _startCapture,
                onPauseCapture: _togglePause,
                onStopAndSave: _stopAndSave,
                onCancelCountdown: () => setState(() => _countdownActive = false),
                onManualCapture: _manualCapture,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
