import 'dart:async';
import 'package:flutter/material.dart';
import '../services/robot_service.dart';

class CameraView extends StatefulWidget {
  final RobotService robotService;

  const CameraView({super.key, required this.robotService});

  @override
  State<CameraView> createState() => _CameraViewState();
}

class _CameraViewState extends State<CameraView> {
  String? _imageUrl;
  bool _showError = false;
  int _consecutiveFailures = 0;
  Timer? _pollTimer;

  @override
  void initState() {
    super.initState();
    _startPolling();
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    super.dispose();
  }

  void _startPolling() {
    _pollTimer = Timer.periodic(const Duration(milliseconds: 150), (_) {
      if (mounted) _fetchSnapshot();
    });
    _fetchSnapshot();
  }

  Future<void> _fetchSnapshot() async {
    final url = widget.robotService.getSnapshotUrl();
    if (!mounted) return;

    setState(() {
      _imageUrl = url;
    });
  }

  void _onImageError() {
    _consecutiveFailures++;
    if (_consecutiveFailures >= 3 && mounted) {
      setState(() {
        _showError = true;
      });
    }
  }

  void _onImageLoaded() {
    _consecutiveFailures = 0;
    if (_showError && mounted) {
      setState(() {
        _showError = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final aspectRatio = 16 / 9;

    if (_showError) {
      return Container(
        decoration: BoxDecoration(
          color: const Color(0xFF1E1E1E),
          borderRadius: BorderRadius.circular(12),
        ),
        child: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Icon(Icons.camera_alt_outlined, size: 48, color: Colors.grey),
              const SizedBox(height: 8),
              const Text(
                'Camera unavailable',
                style: TextStyle(color: Colors.grey, fontSize: 16),
              ),
            ],
          ),
        ),
      );
    }

    return AspectRatio(
      aspectRatio: aspectRatio,
      child: Container(
        decoration: BoxDecoration(
          color: const Color(0xFF1E1E1E),
          borderRadius: BorderRadius.circular(12),
        ),
        child: ClipRRect(
          borderRadius: BorderRadius.circular(12),
          child: _imageUrl != null
              ? Image.network(
                  _imageUrl!,
                  fit: BoxFit.cover,
                  errorBuilder: (context, error, stackTrace) {
                    _onImageError();
                    return const SizedBox.shrink();
                  },
                  loadingBuilder: (context, child, loadingProgress) {
                    if (loadingProgress == null) {
                      _onImageLoaded();
                      return child;
                    }
                    return const SizedBox.shrink();
                  },
                )
              : const SizedBox.shrink(),
        ),
      ),
    );
  }
}
