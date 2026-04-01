import 'dart:typed_data';
import 'package:flutter/material.dart';
import '../services/robot_service.dart';

class CameraView extends StatefulWidget {
  final RobotService robotService;

  const CameraView({super.key, required this.robotService});

  @override
  State<CameraView> createState() => _CameraViewState();
}

class _CameraViewState extends State<CameraView> {
  Uint8List? _frameBytes;
  bool _showError = false;
  int _consecutiveFailures = 0;
  bool _disposed = false;

  @override
  void initState() {
    super.initState();
    _fetchLoop();
  }

  @override
  void dispose() {
    _disposed = true;
    super.dispose();
  }

  Future<void> _fetchLoop() async {
    while (!_disposed) {
      final bytes = await widget.robotService.fetchSnapshotBytes();

      if (_disposed) break;

      if (bytes != null) {
        _consecutiveFailures = 0;
        if (mounted) {
          setState(() {
            _frameBytes = bytes;
            _showError = false;
          });
        }
        // Rely purely on network round-trip time for pacing instead of artificial delay
      } else {
        _consecutiveFailures++;
        if (mounted && _consecutiveFailures >= 3) {
          setState(() {
            _frameBytes = null; // Only clear frame when truly disconnected
            _showError = true;
          });
        }
        // Back off slightly on error
        await Future.delayed(const Duration(milliseconds: 100));
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
