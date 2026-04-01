import 'dart:io';
import 'dart:typed_data';

import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';

import 'robot_service.dart';

const int captureIntervalMs = 1000;
const int countdownSeconds = 3;
const int targetPerClass = 300;
const String _datasetRoot = 'TinyRobot_Dataset';

class CaptureService {
  final RobotService robotService;

  CaptureService({required this.robotService});

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
      final pad = (int n) => n.toString().padLeft(2, '0');
      final timestamp =
          '${now.year}${pad(now.month)}${pad(now.day)}_'
          '${pad(now.hour)}${pad(now.minute)}${pad(now.second)}_'
          '${now.millisecond}';
      final file = File('${dir.path}/${className}_$timestamp.jpg');
      await file.writeAsBytes(bytes);
      return true;
    } on FileSystemException {
      return false;
    }
  }

  Future<int> countImages(String className) async {
    try {
      final dir = await _classDirectory(className);
      final files = dir.listSync().whereType<File>().toList();
      return files.length;
    } on FileSystemException {
      return 0;
    }
  }
}
