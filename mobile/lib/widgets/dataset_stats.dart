import 'package:flutter/material.dart';

import '../services/dense_capture_service.dart';

class DatasetStatsBar extends StatelessWidget {
  final SessionStats stats;

  const DatasetStatsBar({super.key, required this.stats});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: const Color(0xFF1E1E1E),
        borderRadius: BorderRadius.circular(10),
      ),
      child: Text(
        'Captured: ${stats.captured} | Skipped: ${stats.skippedTotal} | WiFi Err: ${stats.wifiErrors}',
        style: const TextStyle(color: Colors.white70, fontSize: 12),
      ),
    );
  }
}

Future<bool> showSessionSummaryDialog(
  BuildContext context,
  SessionStats stats,
  Map<String, int> totals,
) async {
  final result = await showDialog<bool>(
    context: context,
    builder: (ctx) => AlertDialog(
      title: const Text('Session Summary'),
      content: Text(
        'Captured this session: ${stats.captured}\n'
        'Skipped: ${stats.skippedFrozen} frozen | ${stats.skippedDark} dark | ${stats.wifiErrors} wifi error\n\n'
        'FORWARD: ${totals['forward'] ?? 0}\n'
        'LEFT: ${totals['left'] ?? 0}\n'
        'RIGHT: ${totals['right'] ?? 0}\n'
        'NOTHING: ${totals['nothing'] ?? 0}',
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(ctx).pop(false),
          child: const Text('DONE'),
        ),
        TextButton(
          onPressed: () => Navigator.of(ctx).pop(true),
          child: const Text('CAPTURE MORE'),
        ),
      ],
    ),
  );
  return result ?? false;
}
