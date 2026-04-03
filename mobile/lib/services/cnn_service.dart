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
        headers: {'Connection': 'close'},
      ).timeout(const Duration(milliseconds: 2000));
      return response.statusCode == 200;
    } catch (_) {
      return false;
    }
  }

  Future<void> stopCnnMode() async {
    try {
      await _client.post(
        Uri.parse('${_robotService.baseUrl}/cnn/stop'),
        headers: {'Connection': 'close'},
      ).timeout(const Duration(milliseconds: 500));
    } catch (_) {
      // Fire and forget
    }
  }

  Future<CnnStatus?> getCnnStatus() async {
    try {
      final response = await _client.get(
        Uri.parse('${_robotService.baseUrl}/cnn/status'),
        headers: {'Connection': 'close'},
      ).timeout(const Duration(milliseconds: 300));
      
      if (response.statusCode == 200) {
        return CnnStatus.fromJson(jsonDecode(response.body));
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
