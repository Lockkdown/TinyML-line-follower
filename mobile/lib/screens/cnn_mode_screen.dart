import 'package:flutter/material.dart';
import '../services/cnn_service.dart';
import '../services/robot_service.dart';

class CnnModeScreen extends StatefulWidget {
  final RobotService robotService;

  const CnnModeScreen({super.key, required this.robotService});

  @override
  State<CnnModeScreen> createState() => _CnnModeScreenState();
}

class _CnnModeScreenState extends State<CnnModeScreen> {
  late final CnnService _cnnService;
  bool _isStarting = true;
  bool _cnnActivated = false;

  @override
  void initState() {
    super.initState();
    _cnnService = CnnService(widget.robotService);
    _startCnnMode();
  }

  @override
  void dispose() {
    _cnnService.dispose();
    super.dispose();
  }

  Future<void> _startCnnMode() async {
    final started = await _cnnService.startCnnMode();
    if (!mounted) {
      return;
    }

    setState(() {
      _isStarting = false;
      _cnnActivated = started;
    });

    if (started) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('CNN mode đã bật, WiFi sẽ tự tắt')),
      );
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Không thể bật CNN mode')),
      );
      Navigator.of(context).pop();
    }
  }

  Future<void> _tryStopCnnMode() async {
    await _cnnService.stopCnnMode();
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Nếu robot vẫn chạy, hãy nhấn reset ESP32')),
    );
  }

  Future<bool> _confirmLeaveScreen() async {
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Robot vẫn chạy CNN mode cho đến khi reset ESP32'),
        ),
      );
    }
    return true;
  }

  @override
  Widget build(BuildContext context) {
    return WillPopScope(
      onWillPop: _confirmLeaveScreen,
      child: Scaffold(
        backgroundColor: const Color(0xFF121212),
        appBar: AppBar(
          backgroundColor: const Color(0xFF1E1E1E),
          foregroundColor: Colors.white,
          title: const Text('CNN MODE'),
          leading: IconButton(
            icon: const Icon(Icons.close),
            onPressed: () async {
              await _confirmLeaveScreen();
              if (mounted) {
                Navigator.of(context).pop();
              }
            },
          ),
        ),
        body: Center(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Container(
              width: double.infinity,
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: const Color(0xFF1E1E1E),
                borderRadius: BorderRadius.circular(12),
              ),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  if (_isStarting)
                    const CircularProgressIndicator()
                  else
                    Icon(
                      _cnnActivated ? Icons.psychology : Icons.error_outline,
                      size: 48,
                      color: _cnnActivated ? Colors.green : Colors.red,
                    ),
                  const SizedBox(height: 16),
                  Text(
                    _isStarting ? 'Đang bật CNN mode...' : 'CNN MODE RUNNING',
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 24,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  const SizedBox(height: 12),
                  const Text(
                    'Robot đang tự lái - WiFi đã tắt.',
                    textAlign: TextAlign.center,
                    style: TextStyle(color: Colors.white70, fontSize: 16),
                  ),
                  const SizedBox(height: 8),
                  const Text(
                    'Nhấn nút reset trên ESP32 để thoát mode này.',
                    textAlign: TextAlign.center,
                    style: TextStyle(color: Colors.orangeAccent, fontSize: 14),
                  ),
                  const SizedBox(height: 16),
                  SizedBox(
                    width: double.infinity,
                    child: ElevatedButton(
                      onPressed: _isStarting ? null : _tryStopCnnMode,
                      style: ElevatedButton.styleFrom(backgroundColor: Colors.red),
                      child: const Text(
                        'TRY STOP CNN',
                        style: TextStyle(color: Colors.white),
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}
