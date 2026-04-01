import 'dart:async';
import 'dart:io';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

class RobotService {
  static const String _keyBaseUrl = 'robot_base_url';
  static const String _defaultBaseUrl = 'http://192.168.43.200';

  String _baseUrl = _defaultBaseUrl;

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
      final response = await http.get(
        Uri.parse('$_baseUrl/'),
      ).timeout(const Duration(milliseconds: 500));
      return response.statusCode == 200;
    } catch (_) {
      return false;
    }
  }

  Future<void> sendCommand(String cmd) async {
    try {
      await http.post(
        Uri.parse('$_baseUrl/$cmd'),
      ).timeout(const Duration(milliseconds: 500));
    } catch (_) {
      // Silently ignore network errors for fire-and-forget behavior
    }
  }
}
