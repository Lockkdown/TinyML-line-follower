import 'package:flutter/material.dart';

import 'class_selector_button.dart';

class DatasetControls extends StatelessWidget {
  final String selectedClass;
  final Map<String, int> counts;
  final bool capturing;
  final bool paused;
  final bool countdownActive;
  final int countdownValue;
  final ValueChanged<String> onClassSelected;
  final VoidCallback onStartCapture;
  final VoidCallback onPauseCapture;
  final VoidCallback onStopAndSave;
  final VoidCallback onCancelCountdown;
  final VoidCallback onManualCapture;

  const DatasetControls({
    super.key,
    required this.selectedClass,
    required this.counts,
    required this.capturing,
    required this.paused,
    required this.countdownActive,
    required this.countdownValue,
    required this.onClassSelected,
    required this.onStartCapture,
    required this.onPauseCapture,
    required this.onStopAndSave,
    required this.onCancelCountdown,
    required this.onManualCapture,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(10),
      decoration: BoxDecoration(
        color: const Color(0xFF1E1E1E),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          _buildClassRow(),
          if (countdownActive) _buildCountdown(context),
          Row(
            children: [
              IconButton(
                onPressed: onManualCapture,
                icon: const Icon(Icons.camera_alt),
                tooltip: 'Manual capture',
                color: Colors.lightBlueAccent,
              ),
              Expanded(
                child: ElevatedButton(
                  onPressed: onStartCapture,
                  child: const Text('START'),
                ),
              ),
              const SizedBox(width: 6),
              Expanded(
                child: ElevatedButton(
                  onPressed: onPauseCapture,
                  child: Text(paused ? 'RESUME' : 'PAUSE'),
                ),
              ),
              const SizedBox(width: 6),
              Expanded(
                child: ElevatedButton(
                  onPressed: onStopAndSave,
                  child: const Text('STOP'),
                ),
              ),
            ],
          ),
          if (capturing)
            const Padding(
              padding: EdgeInsets.only(top: 6),
              child: Text(
                'Capturing...',
                style: TextStyle(color: Colors.greenAccent),
              ),
            ),
        ],
      ),
    );
  }

  Widget _buildClassRow() {
    const names = ['forward', 'left', 'right', 'nothing'];
    return SizedBox(
      height: 72,
      child: Row(
        children: names
            .map(
              (name) => Expanded(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 2),
                  child: ClassSelectorButton(
                    label: name,
                    count: counts[name] ?? 0,
                    isActive: selectedClass == name,
                    onTap: () => onClassSelected(name),
                  ),
                ),
              ),
            )
            .toList(),
      ),
    );
  }

  Widget _buildCountdown(BuildContext context) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Text(
          'Get ready... $countdownValue',
          style: const TextStyle(fontSize: 20, color: Colors.white),
        ),
        TextButton(onPressed: onCancelCountdown, child: const Text('CANCEL')),
      ],
    );
  }
}
