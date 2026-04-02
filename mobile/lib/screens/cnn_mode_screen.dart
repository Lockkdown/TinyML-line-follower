import 'dart:async';
import 'package:flutter/material.dart';
import '../services/cnn_service.dart';
import '../services/robot_service.dart';
import '../widgets/camera_view.dart';

class CnnModeScreen extends StatefulWidget {
  final RobotService robotService;

  const CnnModeScreen({super.key, required this.robotService});

  @override
  State<CnnModeScreen> createState() => _CnnModeScreenState();
}

class _CnnModeScreenState extends State<CnnModeScreen> {
  late final CnnService _cnnService;
  CnnStatus? _status;
  Timer? _pollTimer;
  int _consecutiveFailures = 0;
  bool _showError = false;
  bool _isPollingStatus = false;

  @override
  void initState() {
    super.initState();
    _cnnService = CnnService(widget.robotService);
    _startCnnMode();
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    _cnnService.stopCnnMode();
    _cnnService.dispose();
    super.dispose();
  }

  Future<void> _startCnnMode() async {
    final started = await _cnnService.startCnnMode();
    if (started) {
      _startPolling();
    } else {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Failed to start CNN mode')),
        );
        Navigator.of(context).pop();
      }
    }
  }

  void _startPolling() {
    _pollTimer = Timer.periodic(const Duration(milliseconds: 400), (_) {
      if (mounted) _pollStatus();
    });
  }

  Future<void> _pollStatus() async {
    if (_isPollingStatus) {
      return;
    }

    _isPollingStatus = true;
    final status = await _cnnService.getCnnStatus();
    if (status != null) {
      _consecutiveFailures = 0;
      if (mounted && _showError) {
        setState(() => _showError = false);
      }
      if (mounted) {
        setState(() => _status = status);
      }
    } else {
      _consecutiveFailures++;
      if (_consecutiveFailures >= 3 && mounted && !_showError) {
        setState(() => _showError = true);
      }
    }
    _isPollingStatus = false;
  }

  Future<void> _stopCnnMode() async {
    await _cnnService.stopCnnMode();
    Navigator.of(context).pop();
  }

  Color _getClassColor(String className) {
    switch (className) {
      case 'forward':
        return Colors.blue;
      case 'left':
      case 'right':
        return Colors.green;
      case 'nothing':
        return Colors.red;
      default:
        return Colors.grey;
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF121212),
      appBar: AppBar(
        backgroundColor: const Color(0xFF1E1E1E),
        foregroundColor: Colors.white,
        title: const Text('CNN MODE'),
        leading: IconButton(
          icon: const Icon(Icons.close),
          onPressed: _stopCnnMode,
        ),
      ),
      body: Padding(
        padding: const EdgeInsets.all(8.0),
        child: Column(
          children: [
            Expanded(
              flex: 3,
              child: CameraView(
                robotService: widget.robotService,
                snapshotTimeout: const Duration(milliseconds: 700),
                minDelayMs: 120,
                maxDelayMs: 600,
              ),
            ),
            const SizedBox(height: 16),
            if (_showError)
              Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: Colors.red.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(8),
                  border: Border.all(color: Colors.red),
                ),
                child: const Row(
                  children: [
                    Icon(Icons.error_outline, color: Colors.red),
                    SizedBox(width: 8),
                    Text(
                      'Connection lost',
                      style: TextStyle(color: Colors.red),
                    ),
                  ],
                ),
              ),
            if (!_showError) ...[
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: const Color(0xFF1E1E1E),
                  borderRadius: BorderRadius.circular(12),
                ),
                child: Column(
                  children: [
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        const Text(
                          'CNN MODE',
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        Container(
                          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                          decoration: BoxDecoration(
                            color: _status?.active == true ? Colors.green : Colors.red,
                            borderRadius: BorderRadius.circular(20),
                          ),
                          child: Text(
                            _status?.active == true ? 'ACTIVE' : 'STOPPED',
                            style: const TextStyle(
                              color: Colors.white,
                              fontSize: 12,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 16),
                    if (_status != null) ...[
                      Text(
                        _status!.className.toUpperCase(),
                        style: TextStyle(
                          color: _getClassColor(_status!.className),
                          fontSize: 32,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      const SizedBox(height: 8),
                      Text(
                        '${(_status!.confidence * 100).toInt()}%',
                        style: const TextStyle(
                          color: Colors.white70,
                          fontSize: 16,
                        ),
                      ),
                      const SizedBox(height: 12),
                      LinearProgressIndicator(
                        value: _status!.confidence,
                        backgroundColor: Colors.grey[700],
                        valueColor: AlwaysStoppedAnimation<Color>(
                          _getClassColor(_status!.className),
                        ),
                      ),
                    ],
                  ],
                ),
              ),
              const SizedBox(height: 16),
              SizedBox(
                width: double.infinity,
                child: ElevatedButton(
                  onPressed: _stopCnnMode,
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.red,
                    padding: const EdgeInsets.symmetric(vertical: 16),
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(8),
                    ),
                  ),
                  child: const Text(
                    'STOP CNN',
                    style: TextStyle(
                      color: Colors.white,
                      fontSize: 18,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}
