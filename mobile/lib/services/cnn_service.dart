import 'dart:convert';
import 'package:http/http.dart' as http;
import 'robot_service.dart';

class CnnStatus {
  final bool active;
  final int classIndex;
  final String className;
  final double confidence;

  const CnnStatus({
    required this.active,
    required this.classIndex,
    required this.className,
    required this.confidence,
  });

  factory CnnStatus.fromJson(Map<String, dynamic> json) {
    return CnnStatus(
      active: json['active'] as bool,
      classIndex: json['class'] as int,
      className: json['class_name'] as String,
      confidence: (json['confidence'] as num).toDouble(),
    );
  }
}

class CnnService {
  final RobotService _robotService;
  final http.Client _client = http.Client();

  CnnService(this._robotService);

  Future<bool> startCnnMode() async {
    try {
      final response = await _client.post(
        Uri.parse('${_robotService.baseUrl}/cnn/start'),
      ).timeout(const Duration(milliseconds: 2000));
      final body = jsonDecode(response.body) as Map<String, dynamic>;
      return body['status'] == 'started' || body['status'] == 'already_active';
    } catch (_) {
      return false;
    }
  }

  Future<bool> stopCnnMode() async {
    try {
      final response = await _client.post(
        Uri.parse('${_robotService.baseUrl}/cnn/stop'),
      ).timeout(const Duration(milliseconds: 2000));
      final body = jsonDecode(response.body) as Map<String, dynamic>;
      return body['status'] == 'stopped';
    } catch (_) {
      return false;
    }
  }

  Future<CnnStatus?> getCnnStatus() async {
    try {
      final response = await _client.get(
        Uri.parse('${_robotService.baseUrl}/cnn/status'),
      ).timeout(const Duration(milliseconds: 1000));
      if (response.statusCode == 200) {
        final body = jsonDecode(response.body) as Map<String, dynamic>;
        return CnnStatus.fromJson(body);
      }
      return null;
    } catch (_) {
      return null;
    }
  }

  void dispose() {
    _client.close();
  }
}
