import 'dart:async';
import 'package:flutter/material.dart';
import '../services/robot_service.dart';
import '../widgets/camera_view.dart';
import '../widgets/control_button.dart';
import 'settings_screen.dart';

class ControllerScreen extends StatefulWidget {
  const ControllerScreen({super.key});

  @override
  State<ControllerScreen> createState() => _ControllerScreenState();
}

class _ControllerScreenState extends State<ControllerScreen> {
  final RobotService _robotService = RobotService();
  bool _connected = false;
  Timer? _connectionTimer;

  @override
  void initState() {
    super.initState();
    _initialize();
  }

  @override
  void dispose() {
    _connectionTimer?.cancel();
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
                  onPressed: _openSettings,
                  icon: const Icon(Icons.settings),
                ),
              ],
            ),
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            Expanded(
              flex: 2,
              child: CameraView(robotService: _robotService),
            ),
            const SizedBox(height: 32),
            Expanded(
              flex: 3,
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  // Forward button
                  ControlButton(
                    backgroundColor: Colors.blue,
                    onPressed: () => _sendCommand('forward'),
                    onReleased: () => _sendCommand('stop'),
                    onCanceled: () => _sendCommand('stop'),
                    child: const Icon(Icons.arrow_upward, color: Colors.white, size: 32),
                  ),
                  const SizedBox(height: 16),
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
                  const SizedBox(height: 16),
                  // Stop button (bottom)
                  ControlButton(
                    backgroundColor: Colors.red,
                    onPressed: () => _sendCommand('stop'),
                    child: const Icon(Icons.stop, color: Colors.white, size: 32),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
