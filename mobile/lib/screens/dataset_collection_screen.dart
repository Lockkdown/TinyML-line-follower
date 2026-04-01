import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/material.dart';

import '../services/capture_service.dart';
import '../services/robot_service.dart';
import '../services/transfer_service.dart';
import '../widgets/class_selector_button.dart';

const List<String> _classNames = ['forward', 'left', 'right', 'nothing'];

class DatasetCollectionScreen extends StatefulWidget {
  final RobotService robotService;

  const DatasetCollectionScreen({super.key, required this.robotService});

  @override
  State<DatasetCollectionScreen> createState() =>
      _DatasetCollectionScreenState();
}

class _DatasetCollectionScreenState extends State<DatasetCollectionScreen> {
  late final CaptureService _captureService;

  String? _activeClass;
  final Map<String, int> _counts = {
    for (final c in _classNames) c: 0,
  };

  bool _countdownActive = false;
  int _countdownValue = countdownSeconds;
  Timer? _countdownTimer;

  bool _captureReady = false;


  Uint8List? _cameraFrame;
  bool _cameraDisposed = false;

  final TransferService _transferService = TransferService();
  bool _serverRunning = false;
  String? _phoneIp;
  List<String> _allIps = [];

  @override
  void initState() {
    super.initState();
    _captureService = CaptureService(robotService: widget.robotService);
    _loadCounts();
    _requestPermissions();
    _startCameraLoop();
    _loadPhoneIp();
  }

  @override
  void dispose() {
    _cameraDisposed = true;
    _countdownTimer?.cancel();
    _transferService.stop();
    super.dispose();
  }

  Future<void> _loadPhoneIp() async {
    final ip = await _transferService.getPhoneIp();
    final all = await _transferService.getAllLanIps();
    if (mounted) setState(() {
      _phoneIp = ip;
      _allIps = all;
    });
  }

  Future<void> _toggleTransferServer() async {
    if (_serverRunning) {
      await _transferService.stop();
      if (mounted) setState(() => _serverRunning = false);
    } else {
      await _transferService.start();
      if (mounted) {
        setState(() => _serverRunning = true);
        _showTransferDialog();
      }
    }
  }

  void _showTransferDialog() {
    final ips = _allIps.isNotEmpty ? _allIps : [_phoneIp ?? '?.?.?.?'];
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
                'Open one of these URLs on your PC browser\n(try each until one works):',
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
                'Click a class link to download its ZIP.\nServer stays running until you tap the WiFi icon again.',
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

  Future<void> _startCameraLoop() async {
    while (!_cameraDisposed) {
      final bytes = await widget.robotService.fetchSnapshotBytes();
      if (_cameraDisposed) break;
      if (bytes != null && mounted) {
        setState(() => _cameraFrame = bytes);
      }
      await Future.delayed(const Duration(milliseconds: 100));
    }
  }

  void _sendCommand(String cmd) {
    widget.robotService.sendCommand(cmd);
  }

  Future<void> _requestPermissions() async {
    await _captureService.requestStoragePermission();
  }

  Future<void> _loadCounts() async {
    final updated = <String, int>{};
    for (final className in _classNames) {
      updated[className] = await _captureService.countImages(className);
    }
    if (mounted) {
      setState(() {
        _counts.addAll(updated);
        if (_activeClass != null && (_counts[_activeClass!] ?? 0) == 0) {
          _activeClass = null;
          _captureReady = false;
          _countdownActive = false;
        }
      });
    }
  }

  void _selectClass(String className) {
    setState(() {
      _activeClass = className;
      _captureReady = false;
      _countdownActive = false;
    });
    _startCountdown();
  }

  void _startCountdown() {
    _countdownTimer?.cancel();
    setState(() {
      _countdownActive = true;
      _countdownValue = countdownSeconds;
    });
    _countdownTimer = Timer.periodic(const Duration(seconds: 1), (timer) {
      if (!mounted) {
        timer.cancel();
        return;
      }
      setState(() => _countdownValue--);
      if (_countdownValue <= 0) {
        timer.cancel();
        setState(() {
          _countdownActive = false;
          _captureReady = true;
        });
      }
    });
  }

  Future<void> _captureOnce() async {
    if (!_captureReady || _activeClass == null) return;
    final bytes = await _captureService.fetchSnapshot();
    if (bytes == null) return;
    final saved = await _captureService.saveImage(bytes, _activeClass!);
    if (saved && mounted) {
      setState(() => _counts[_activeClass!] = (_counts[_activeClass!] ?? 0) + 1);
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Image saved'),
          duration: Duration(seconds: 1),
          backgroundColor: Colors.green,
        ),
      );
    }
  }

  void _resetClass() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: const Color(0xFF1E1E1E),
        title: const Text('Reset', style: TextStyle(color: Colors.white)),
        content: const Text(
          'Rescan image counts for all classes?',
          style: TextStyle(color: Colors.grey),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('Rescan', style: TextStyle(color: Colors.blue)),
          ),
        ],
      ),
    );
    if (confirmed == true) {
      setState(() => _activeClass = null);
      await _loadCounts();
    }
  }

  int get _totalImages => _counts.values.fold(0, (a, b) => a + b);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF121212),
      appBar: AppBar(
        backgroundColor: const Color(0xFF1E1E1E),
        foregroundColor: Colors.white,
        title: const Text('DATASET COLLECTION'),
        actions: [
          IconButton(
            icon: Icon(_serverRunning ? Icons.wifi_off : Icons.wifi),
            tooltip: _serverRunning ? 'Stop Transfer Server' : 'WiFi Transfer',
            color: _serverRunning ? Colors.greenAccent : null,
            onPressed: _toggleTransferServer,
          ),
          IconButton(
            icon: const Icon(Icons.refresh),
            tooltip: 'Reset / Rescan',
            onPressed: _resetClass,
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(12.0),
        child: Column(
          children: [
            Row(
              children: [
                Expanded(child: _buildProgressCard()),
                const SizedBox(width: 8),
                IconButton(
                  onPressed: _loadCounts,
                  icon: const Icon(Icons.refresh, size: 20),
                  tooltip: 'Rescan image counts',
                  style: IconButton.styleFrom(
                    backgroundColor: const Color(0xFF1E1E1E),
                    foregroundColor: Colors.white,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Expanded(
              flex: 3,
              child: _buildCameraPreview(),
            ),
            const SizedBox(height: 8),
            Expanded(
              flex: 4,
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Expanded(flex: 3, child: _buildClassGrid()),
                  const SizedBox(width: 8),
                  SizedBox(width: 120, child: _buildMiniDpad()),
                ],
              ),
            ),
            const SizedBox(height: 8),
            _buildCaptureArea(),
          ],
        ),
      ),
    );
  }

  Widget _buildCameraPreview() {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF1E1E1E),
        borderRadius: BorderRadius.circular(12),
      ),
      clipBehavior: Clip.antiAlias,
      child: _cameraFrame != null
          ? Image.memory(
              _cameraFrame!,
              fit: BoxFit.cover,
              width: double.infinity,
              gaplessPlayback: true,
              errorBuilder: (_, __, ___) => _buildCameraPlaceholder(),
            )
          : _buildCameraPlaceholder(),
    );
  }

  Widget _buildCameraPlaceholder() {
    return const Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(Icons.camera_alt_outlined, size: 32, color: Colors.grey),
          SizedBox(height: 4),
          Text('Camera unavailable', style: TextStyle(color: Colors.grey, fontSize: 12)),
        ],
      ),
    );
  }

  Widget _buildMiniDpad() {
    return Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        _buildDpadButton(Icons.arrow_upward, 'forward', 'stop'),
        const SizedBox(height: 6),
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            _buildDpadButton(Icons.arrow_back, 'left', 'stop'),
            const SizedBox(width: 6),
            _buildDpadButton(Icons.stop, 'stop', null, color: Colors.red),
            const SizedBox(width: 6),
            _buildDpadButton(Icons.arrow_forward, 'right', 'stop'),
          ],
        ),
        const SizedBox(height: 6),
        _buildDpadButton(Icons.arrow_downward, 'backward', 'stop', color: Colors.orange),
      ],
    );
  }

  Widget _buildDpadButton(
    IconData icon,
    String pressCmd,
    String? releaseCmd, {
    Color color = Colors.blueGrey,
  }) {
    return GestureDetector(
      onTapDown: (_) => _sendCommand(pressCmd),
      onTapUp: (_) { if (releaseCmd != null) _sendCommand(releaseCmd); },
      onTapCancel: () { if (releaseCmd != null) _sendCommand(releaseCmd); },
      child: Container(
        width: 34,
        height: 34,
        decoration: BoxDecoration(
          color: color.withOpacity(0.85),
          borderRadius: BorderRadius.circular(8),
        ),
        child: Icon(icon, color: Colors.white, size: 18),
      ),
    );
  }

  Widget _buildProgressCard() {
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 16),
      decoration: BoxDecoration(
        color: const Color(0xFF1E1E1E),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Column(
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceAround,
            children: [
              _buildCountCell('FORWARD', _counts['forward'] ?? 0),
              _buildCountCell('LEFT', _counts['left'] ?? 0),
              _buildCountCell('RIGHT', _counts['right'] ?? 0),
              _buildCountCell('NOTHING', _counts['nothing'] ?? 0),
            ],
          ),
          const Divider(color: Colors.grey, height: 16),
          Text(
            'Total: $_totalImages / ${_classNames.length * targetPerClass}',
            style: const TextStyle(
              color: Colors.white70,
              fontSize: 13,
              fontWeight: FontWeight.w500,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildCountCell(String label, int count) {
    return Column(
      children: [
        Text(label,
            style: const TextStyle(color: Colors.grey, fontSize: 11)),
        const SizedBox(height: 2),
        Text(
          '$count',
          style: const TextStyle(
            color: Colors.white,
            fontSize: 18,
            fontWeight: FontWeight.bold,
          ),
        ),
      ],
    );
  }

  Widget _buildClassGrid() {
    final topRow = _classNames.sublist(0, 2);
    final bottomRow = _classNames.sublist(2, 4);
    return Stack(
      fit: StackFit.expand,
      children: [
        Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Expanded(
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Expanded(child: _buildClassCell(topRow[0])),
                  const SizedBox(width: 8),
                  Expanded(child: _buildClassCell(topRow[1])),
                ],
              ),
            ),
            const SizedBox(height: 8),
            Expanded(
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Expanded(child: _buildClassCell(bottomRow[0])),
                  const SizedBox(width: 8),
                  Expanded(child: _buildClassCell(bottomRow[1])),
                ],
              ),
            ),
          ],
        ),
        if (_countdownActive) _buildCountdownOverlay(),
      ],
    );
  }

  Widget _buildClassCell(String className) {
    return ClassSelectorButton(
      label: className,
      count: _counts[className] ?? 0,
      isActive: _activeClass == className,
      onTap: () => _selectClass(className),
    );
  }

  Widget _buildCountdownOverlay() {
    return Container(
      decoration: BoxDecoration(
        color: Colors.black.withOpacity(0.75),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Text(
              'Get ready...',
              style: TextStyle(color: Colors.white70, fontSize: 16),
            ),
            const SizedBox(height: 8),
            Text(
              '$_countdownValue',
              style: const TextStyle(
                color: Colors.white,
                fontSize: 64,
                fontWeight: FontWeight.bold,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildCaptureArea() {
    return GestureDetector(
      onTap: _captureOnce,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        decoration: BoxDecoration(
          color: _captureReady ? Colors.green.shade700 : Colors.grey.shade800,
          borderRadius: BorderRadius.circular(16),
        ),
        padding: const EdgeInsets.symmetric(vertical: 24),
        child: Stack(
          alignment: Alignment.center,
          children: [
            const Icon(
              Icons.camera_alt,
              size: 32,
              color: Colors.white,
            ),
            if (_countdownActive)
              Positioned(
                top: 8,
                child: Text(
                  _countdownValue.toString(),
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ),
            if (!_captureReady && !_countdownActive)
              const Positioned(
                bottom: 8,
                child: Text(
                  'Select a class',
                  style: TextStyle(
                    color: Colors.grey,
                    fontSize: 12,
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }
}
