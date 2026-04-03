import 'dart:async';
import 'package:flutter/material.dart';
import '../services/robot_service.dart';
import '../widgets/camera_view.dart';
import '../widgets/control_button.dart';
import 'dataset_collection_screen.dart';
import 'settings_screen.dart';
import 'cnn_mode_screen.dart';

class ControllerScreen extends StatefulWidget {
  const ControllerScreen({super.key});

  @override
  State<ControllerScreen> createState() => _ControllerScreenState();
}

class _ControllerScreenState extends State<ControllerScreen> {
  final RobotService _robotService = RobotService();
  bool _connected = false;
  bool _cameraEnabled = false;
  bool _isCameraToggleBusy = false;
  Timer? _connectionTimer;
  double _speed = 100;

  @override
  void initState() {
    super.initState();
    _initialize();
  }

  @override
  void dispose() {
    _connectionTimer?.cancel();
    _robotService.dispose();
    super.dispose();
  }

  Future<void> _initialize() async {
    await _robotService.loadBaseUrl();
    await _checkConnection();
    _startConnectionPolling();
  }

  void _startConnectionPolling() {
    _connectionTimer = Timer.periodic(const Duration(seconds: 2), (_) {
      if (mounted) _checkConnection();
    });
  }

  Future<void> _checkConnection() async {
    final connected = await _robotService.checkConnection();
    if (mounted && connected != _connected) {
      setState(() {
        _connected = connected;
      });
    }
  }

  void _sendCommand(String cmd) {
    _robotService.sendCommand(cmd);
  }

  Future<void> _toggleCameraMode() async {
    if (_isCameraToggleBusy) {
      return;
    }

    setState(() {
      _isCameraToggleBusy = true;
    });

    final bool success = _cameraEnabled
        ? await _robotService.setCameraOff()
        : await _robotService.setCameraOn();

    if (!mounted) {
      return;
    }

    if (success) {
      setState(() {
        _cameraEnabled = !_cameraEnabled;
      });
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Không thể đổi trạng thái camera')),
      );
    }

    setState(() {
      _isCameraToggleBusy = false;
    });
  }

  Widget _buildCameraPanel() {
    if (_cameraEnabled) {
      return CameraView(robotService: _robotService);
    }

    return AspectRatio(
      aspectRatio: 16 / 9,
      child: Container(
        decoration: BoxDecoration(
          color: const Color(0xFF1E1E1E),
          borderRadius: BorderRadius.circular(12),
        ),
        child: const Center(
          child: Text(
            'Camera OFF - tap icon to enable',
            style: TextStyle(color: Colors.grey, fontSize: 14),
          ),
        ),
      ),
    );
  }

  Widget _buildSpeedControl() {
    return Column(
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Text(
              'Speed: ',
              style: TextStyle(color: Colors.white70, fontSize: 14),
            ),
            Text(
              _speed.toInt().toString(),
              style: const TextStyle(
                color: Colors.white,
                fontSize: 14,
                fontWeight: FontWeight.bold,
              ),
            ),
          ],
        ),
        SizedBox(
          height: 36, // Ép chiều cao slider mỏng lại để tiết kiệm diện tích dọc
          child: Slider(
            value: _speed,
            min: 40,
            max: 255,
            divisions: 43,
            activeColor: Colors.blueAccent,
            inactiveColor: Colors.grey[800],
            onChanged: (value) {
              setState(() {
                _speed = value;
              });
            },
            onChangeEnd: (value) {
              _robotService.setSpeed(value.toInt());
            },
          ),
        ),
      ],
    );
  }

  void _openDatasetCollection() {
    Navigator.of(context)
        .push<void>(
          MaterialPageRoute(
            builder: (context) =>
                DatasetCollectionScreen(robotService: _robotService),
          ),
        )
        .then((_) {
          if (mounted) {
            setState(() => _cameraEnabled = false);
          }
        });
  }

  void _openCnnMode() {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Bật CNN Mode'),
        content: const Text('Xe sẽ tự lái dựa trên camera.\n\nBạn có chắc chắn?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Huỷ'),
          ),
          TextButton(
            onPressed: () {
              Navigator.of(context).pop();
              Navigator.of(context).push(
                MaterialPageRoute(
                  builder: (context) => CnnModeScreen(robotService: _robotService),
                ),
              );
            },
            child: const Text('Bật CNN'),
          ),
        ],
      ),
    );
  }

  void _openSettings() async {
    final result = await Navigator.of(context).push<String>(
      MaterialPageRoute(
        builder: (context) => SettingsScreen(
          initialUrl: _robotService.baseUrl,
          onUrlChanged: (url) async {
            await _robotService.saveBaseUrl(url);
            await _checkConnection();
          },
        ),
      ),
    );
    if (result != null && mounted) {
      await _checkConnection();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF121212),
      appBar: AppBar(
        backgroundColor: const Color(0xFF1E1E1E),
        foregroundColor: Colors.white,
        title: const Text('ROBOT CONTROLLER'),
        actions: [
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16.0),
            child: Row(
              children: [
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                  decoration: BoxDecoration(
                    color: _connected ? Colors.green : Colors.red,
                    borderRadius: BorderRadius.circular(20),
                  ),
                  child: Text(
                    _connected ? 'Connected' : 'Disconnected',
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 12,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                ),
                const SizedBox(width: 8),
                IconButton(
                  onPressed: _toggleCameraMode,
                  icon: _isCameraToggleBusy
                      ? const SizedBox(
                          width: 18,
                          height: 18,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : Icon(_cameraEnabled ? Icons.videocam : Icons.videocam_off),
                  tooltip: _cameraEnabled ? 'Camera ON' : 'Camera OFF',
                ),
                IconButton(
                  onPressed: _openDatasetCollection,
                  icon: const Icon(Icons.dataset),
                  tooltip: 'Dataset Collection',
                ),
                IconButton(
                  onPressed: _openCnnMode,
                  icon: const Icon(Icons.psychology),
                  tooltip: 'CNN Mode',
                ),
                IconButton(
                  onPressed: _openSettings,
                  icon: const Icon(Icons.settings),
                ),
              ],
            ),
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(8.0),
        child: Column(
          children: [
            Expanded(
              flex: 5, // Tăng không gian cho control để không bị cuộn/tràn
              child: SingleChildScrollView(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    _buildCameraPanel(),
                    const SizedBox(height: 16),
                    _buildSpeedControl(),
                    const SizedBox(height: 12),
                    // Forward button
                    ControlButton(
                      backgroundColor: Colors.blue,
                      onPressed: () => _sendCommand('forward'),
                      onReleased: () => _sendCommand('stop'),
                      onCanceled: () => _sendCommand('stop'),
                      child: const Icon(Icons.arrow_upward, color: Colors.white, size: 32),
                    ),
                    const SizedBox(height: 12), // Khoảng cách đều 12px
                    // Left, Stop, Right buttons
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                      children: [
                        ControlButton(
                          backgroundColor: Colors.green,
                          onPressed: () => _sendCommand('left'),
                          onReleased: () => _sendCommand('stop'),
                          onCanceled: () => _sendCommand('stop'),
                          child: const Icon(Icons.arrow_back, color: Colors.white, size: 32),
                        ),
                        ControlButton(
                          backgroundColor: Colors.red,
                          onPressed: () => _sendCommand('stop'),
                          child: const Icon(Icons.stop, color: Colors.white, size: 32),
                        ),
                        ControlButton(
                          backgroundColor: Colors.green,
                          onPressed: () => _sendCommand('right'),
                          onReleased: () => _sendCommand('stop'),
                          onCanceled: () => _sendCommand('stop'),
                          child: const Icon(Icons.arrow_forward, color: Colors.white, size: 32),
                        ),
                      ],
                    ),
                    const SizedBox(height: 12), // Khoảng cách đều 12px
                    // Backward button (bottom)
                    ControlButton(
                      backgroundColor: Colors.orange,
                      onPressed: () => _sendCommand('backward'),
                      onReleased: () => _sendCommand('stop'),
                      onCanceled: () => _sendCommand('stop'),
                      child: const Icon(Icons.arrow_downward, color: Colors.white, size: 32),
                    ),
                    const SizedBox(height: 8), // Padding mỏng dưới cùng
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
