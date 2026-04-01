import 'dart:io';
import 'dart:typed_data';

import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'robot_service.dart';

const int captureIntervalMs = 1000;
const int countdownSeconds = 3;
const int targetPerClass = 300;
const String _datasetRoot = 'TinyRobot_Dataset';

class CaptureService {
  final RobotService robotService;
  late SharedPreferences _prefs;

  CaptureService({required this.robotService});

  Future<void> init() async {
    _prefs = await SharedPreferences.getInstance();
  }

  Future<bool> requestStoragePermission() async {
    if (Platform.isAndroid) {
      final status = await Permission.manageExternalStorage.request();
      if (status.isGranted) return true;
      final legacy = await Permission.storage.request();
      return legacy.isGranted;
    }
    return true;
  }

  Future<Directory> _classDirectory(String className) async {
    final external = await getExternalStorageDirectory();
    final pictures = Directory(
      '${external!.path.split('Android').first}Pictures/$_datasetRoot/raw/$className',
    );
    if (!await pictures.exists()) await pictures.create(recursive: true);
    return pictures;
  }

  Future<Uint8List?> fetchSnapshot() async {
    return robotService.fetchSnapshotBytes();
  }

  Future<bool> saveImage(Uint8List bytes, String className) async {
    try {
      final dir = await _classDirectory(className);
      final now = DateTime.now();
      final date = '${now.year}${now.month.toString().padLeft(2, '0')}${now.day.toString().padLeft(2, '0')}';
      final seq = _getNextSequence(className, date);
      final file = File('${dir.path}/${className}_${date}_$seq.jpg');
      await file.writeAsBytes(bytes);
      return true;
    } on FileSystemException {
      return false;
    }
  }

  int _getNextSequence(String className, String date) {
    final key = 'seq_${className}_$date';
    final seq = _prefs.getInt(key) ?? 0;
    _prefs.setInt(key, seq + 1);
    return seq + 1;
  }

  Future<int> countImages(String className) async {
    try {
      final dir = await _classDirectory(className);
      final files = dir.listSync().whereType<File>().toList();
      if (files.isEmpty) {
        // Reset all sequence counters for this class
        final keys = _prefs.getKeys().where((k) => k.startsWith('seq_$className'));
        for (final key in keys) {
          await _prefs.remove(key);
        }
      }
      return files.length;
    } on FileSystemException {
      return 0;
    }
  }
}
