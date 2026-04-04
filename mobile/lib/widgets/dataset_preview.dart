import 'dart:typed_data';

import 'package:flutter/material.dart';

import 'zone_overlay_painter.dart';

class DatasetPreview extends StatelessWidget {
  final Uint8List? frame;
  final int captured;
  final int skipped;
  final String className;
  final int classCount;
  final int targetPerClass;

  const DatasetPreview({
    super.key,
    required this.frame,
    required this.captured,
    required this.skipped,
    required this.className,
    required this.classCount,
    required this.targetPerClass,
  });

  @override
  Widget build(BuildContext context) {
    final progress = (classCount / targetPerClass).clamp(0.0, 1.0);
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF1E1E1E),
        borderRadius: BorderRadius.circular(12),
      ),
      clipBehavior: Clip.antiAlias,
      child: Stack(
        children: [
          Positioned.fill(child: _buildFrame()),
          Positioned.fill(
            child: IgnorePointer(
              child: CustomPaint(painter: ZoneOverlayPainter()),
            ),
          ),
          Positioned(
            left: 8,
            right: 8,
            bottom: 8,
            child: Container(
              padding: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: Colors.black.withOpacity(0.6),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Column(
                children: [
                  Text(
                    'Captured: $captured | Skipped: $skipped | Class: ${className.toUpperCase()}',
                    style: const TextStyle(color: Colors.white, fontSize: 12),
                  ),
                  const SizedBox(height: 6),
                  LinearProgressIndicator(
                    value: progress,
                    color: Colors.lightBlueAccent,
                    backgroundColor: Colors.white24,
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildFrame() {
    if (frame == null) {
      return const Center(
        child: Text(
          'No captured frame yet',
          style: TextStyle(color: Colors.grey),
        ),
      );
    }
    return Image.memory(frame!, fit: BoxFit.cover, gaplessPlayback: true);
  }
}
