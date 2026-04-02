import 'dart:typed_data';
import 'package:flutter/material.dart';
import '../services/robot_service.dart';

class CameraView extends StatefulWidget {
  final RobotService robotService;
  final Duration snapshotTimeout;
  final int minDelayMs;
  final int maxDelayMs;

  const CameraView({
    super.key,
    required this.robotService,
    this.snapshotTimeout = const Duration(milliseconds: 300),
    this.minDelayMs = 33,
    this.maxDelayMs = 250,
  });

  @override
  State<CameraView> createState() => _CameraViewState();
}

class _CameraViewState extends State<CameraView> {
  Uint8List? _frameBytes;
  bool _showError = false;
  int _consecutiveFailures = 0;
  bool _disposed = false;
  late int _currentDelay;
  int _backoffMultiplier = 1;

  @override
  void initState() {
    super.initState();
    _currentDelay = widget.minDelayMs;
    _fetchLoop();
  }

  @override
  void dispose() {
    _disposed = true;
    super.dispose();
  }

  Future<void> _fetchLoop() async {
    while (!_disposed) {
      final startTime = DateTime.now();
      final bytes = await widget.robotService.fetchSnapshotBytes(
        timeout: widget.snapshotTimeout,
      );

      if (_disposed) break;

      if (bytes != null) {
        _consecutiveFailures = 0;
        _backoffMultiplier = 1;

        if (_currentDelay > widget.minDelayMs) {
          _currentDelay = (_currentDelay - 10).clamp(widget.minDelayMs, widget.maxDelayMs);
        }

        if (mounted) {
          setState(() {
            _frameBytes = bytes;
            _showError = false;
          });
        }

        final elapsed = DateTime.now().difference(startTime).inMilliseconds;
        final adjustedDelay = (_currentDelay - elapsed).clamp(10, _currentDelay);
        await Future.delayed(Duration(milliseconds: adjustedDelay));
      } else {
        _consecutiveFailures++;

        if (_consecutiveFailures >= 2) {
          _backoffMultiplier = (_backoffMultiplier * 2).clamp(1, 8);
          _currentDelay = (widget.minDelayMs * _backoffMultiplier).clamp(widget.minDelayMs, widget.maxDelayMs);
        }

        if (mounted && _consecutiveFailures >= 5) {
          setState(() {
            _frameBytes = null;
            _showError = true;
          });
        }
        
        await Future.delayed(Duration(milliseconds: _currentDelay));
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_showError) {
      return _buildErrorState();
    }

    return AspectRatio(
      aspectRatio: 16 / 9,
      child: Container(
        decoration: BoxDecoration(
          color: const Color(0xFF1E1E1E),
          borderRadius: BorderRadius.circular(12),
        ),
        child: ClipRRect(
          borderRadius: BorderRadius.circular(12),
          child: _frameBytes != null
              ? Image.memory(
                  _frameBytes!,
                  fit: BoxFit.cover,
                  gaplessPlayback: true,
                  errorBuilder: (context, error, stackTrace) {
                    return const SizedBox.shrink();
                  },
                )
              : const SizedBox.shrink(),
        ),
      ),
    );
  }

  Widget _buildErrorState() {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF1E1E1E),
        borderRadius: BorderRadius.circular(12),
      ),
      child: const Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.camera_alt_outlined, size: 48, color: Colors.grey),
            SizedBox(height: 8),
            Text(
              'Camera unavailable',
              style: TextStyle(color: Colors.grey, fontSize: 16),
            ),
          ],
        ),
      ),
    );
  }
}
