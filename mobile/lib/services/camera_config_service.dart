import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

class CameraSettings {
  final int quality;
  final String resolution;
  final int brightness;

  const CameraSettings({
    required this.quality,
    required this.resolution,
    required this.brightness,
  });

  Map<String, dynamic> toJson() {
    return {
      'quality': quality,
      'resolution': resolution,
      'brightness': brightness,
    };
  }
}

class CameraConfigService {
  final http.Client _client = http.Client();

  /// Returns `null` on success; on failure returns a short message (HTTP code or error kind).
  Future<String?> applyCameraSettings(
    CameraSettings settings,
    String baseUrl,
  ) async {
    try {
      final response = await _client
          .post(
            Uri.parse('$baseUrl/camera-config'),
            headers: {
              'Content-Type': 'application/json',
              'Connection': 'close',
            },
            body: jsonEncode(settings.toJson()),
          )
          .timeout(const Duration(milliseconds: 1000));
      if (response.statusCode == 200) return null;
      return 'HTTP ${response.statusCode}';
    } on TimeoutException {
      return 'timeout';
    } catch (_) {
      return 'network';
    }
  }

  void dispose() {
    _client.close();
  }
}
