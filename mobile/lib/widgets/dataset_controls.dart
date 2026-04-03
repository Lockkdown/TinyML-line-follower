import 'package:flutter/material.dart';

import '../services/camera_config_service.dart';
import 'class_selector_button.dart';

class DatasetControls extends StatelessWidget {
  final String selectedClass;
  final Map<String, int> counts;
  final CameraSettings settings;
  final int intervalMs;
  final bool capturing;
  final bool paused;
  final bool countdownActive;
  final int countdownValue;
  final ValueChanged<String> onClassSelected;
  final ValueChanged<CameraSettings> onSettingsChanged;
  final VoidCallback onApplySettings;
  final ValueChanged<int> onIntervalChanged;
  final VoidCallback onStartCapture;
  final VoidCallback onPauseCapture;
  final VoidCallback onStopAndSave;
  final VoidCallback onCancelCountdown;
  final VoidCallback onManualCapture;

  const DatasetControls({
    super.key,
    required this.selectedClass,
    required this.counts,
    required this.settings,
    required this.intervalMs,
    required this.capturing,
    required this.paused,
    required this.countdownActive,
    required this.countdownValue,
    required this.onClassSelected,
    required this.onSettingsChanged,
    required this.onApplySettings,
    required this.onIntervalChanged,
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
      child: SingleChildScrollView(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            _buildClassRow(),
            ExpansionTile(
              tilePadding: EdgeInsets.zero,
              title: const Text('Camera Settings'),
              children: [
                _buildQualitySlider(),
                _buildResolution(),
                _buildBrightnessSlider(),
                ElevatedButton(
                  onPressed: onApplySettings,
                  child: const Text('Apply'),
                ),
              ],
            ),
            _buildIntervalSlider(),
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

  Widget _buildQualitySlider() {
    return Slider(
      value: settings.quality.toDouble(),
      min: 10,
      max: 40,
      divisions: 30,
      label: 'JPEG ${settings.quality}',
      onChanged: (value) => onSettingsChanged(
        CameraSettings(
          quality: value.toInt(),
          resolution: settings.resolution,
          brightness: settings.brightness,
        ),
      ),
    );
  }

  Widget _buildResolution() {
    return DropdownButton<String>(
      value: settings.resolution,
      items: const [
        DropdownMenuItem(value: 'QQVGA', child: Text('QQVGA (160x120)')),
        DropdownMenuItem(value: 'QVGA', child: Text('QVGA (320x240)')),
      ],
      onChanged: (value) {
        if (value == null) return;
        onSettingsChanged(
          CameraSettings(
            quality: settings.quality,
            resolution: value,
            brightness: settings.brightness,
          ),
        );
      },
    );
  }

  Widget _buildBrightnessSlider() {
    return Slider(
      value: settings.brightness.toDouble(),
      min: -2,
      max: 2,
      divisions: 4,
      label: 'Brightness ${settings.brightness}',
      onChanged: (value) => onSettingsChanged(
        CameraSettings(
          quality: settings.quality,
          resolution: settings.resolution,
          brightness: value.toInt(),
        ),
      ),
    );
  }

  Widget _buildIntervalSlider() {
    return Slider(
      value: intervalMs.toDouble(),
      min: 200,
      max: 800,
      divisions: 12,
      label: '${intervalMs}ms',
      onChanged: (value) => onIntervalChanged(value.toInt()),
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
