import 'dart:async';
import 'dart:io';
import 'dart:typed_data';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

class RobotService {
  static const String _keyBaseUrl = 'robot_base_url';
  static const String _defaultBaseUrl = 'http://10.64.104.208';

  String _baseUrl = _defaultBaseUrl;
  final http.Client _client = http.Client();

  Future<void> loadBaseUrl() async {
    final prefs = await SharedPreferences.getInstance();
    _baseUrl = prefs.getString(_keyBaseUrl) ?? _defaultBaseUrl;
  }

  Future<void> saveBaseUrl(String value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_keyBaseUrl, value);
    _baseUrl = value;
  }

  String get baseUrl => _baseUrl;

  String getSnapshotUrl() {
    return '$_baseUrl/snapshot?t=${DateTime.now().millisecondsSinceEpoch}';
  }

  Future<bool> checkConnection() async {
    try {
      final response = await _client.get(
        Uri.parse('$_baseUrl/'),
      ).timeout(const Duration(milliseconds: 500));
      return response.statusCode == 200;
    } catch (_) {
      return false;
    }
  }

  Future<Uint8List?> fetchSnapshotBytes() async {
    try {
      final response = await _client.get(
        Uri.parse(getSnapshotUrl()),
      ).timeout(const Duration(milliseconds: 1500));
      if (response.statusCode == 200) return response.bodyBytes;
      return null;
    } catch (_) {
      return null;
    }
  }

  Future<void> sendCommand(String cmd) async {
    try {
      await _client.post(
        Uri.parse('$_baseUrl/$cmd'),
      ).timeout(const Duration(milliseconds: 500));
    } catch (_) {
      // Silently ignore network errors for fire-and-forget behavior
    }
  }

  void dispose() {
    _client.close();
  }
}
