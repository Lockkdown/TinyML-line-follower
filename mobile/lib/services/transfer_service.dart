import 'dart:io';
import 'dart:typed_data';

import 'package:archive/archive.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:path_provider/path_provider.dart';
import 'package:shelf/shelf.dart';
import 'package:shelf/shelf_io.dart' as shelf_io;

const int transferServerPort = 8080;
const String _datasetRoot = 'TinyRobot_Dataset';
const List<String> classNames = ['forward', 'left', 'right', 'nothing'];

class TransferService {
  HttpServer? _server;

  bool get isRunning => _server != null;

  Future<String?> getPhoneIp() async {
    try {
      final interfaces = await NetworkInterface.list(
        type: InternetAddressType.IPv4,
        includeLoopback: false,
      );
      for (final iface in interfaces) {
        final isWifi = iface.name.toLowerCase().contains('wlan') ||
            iface.name.toLowerCase().contains('wifi') ||
            iface.name.toLowerCase().contains('wl');
        for (final addr in iface.addresses) {
          if (_isLanAddress(addr.address)) {
            if (isWifi) return addr.address;
          }
        }
      }
      // Fallback: return any LAN address found
      for (final iface in interfaces) {
        for (final addr in iface.addresses) {
          if (_isLanAddress(addr.address)) return addr.address;
        }
      }
    } catch (_) {}
    return NetworkInfo().getWifiIP();
  }

  Future<List<String>> getAllLanIps() async {
    final result = <String>[];
    try {
      final interfaces = await NetworkInterface.list(
        type: InternetAddressType.IPv4,
        includeLoopback: false,
      );
      for (final iface in interfaces) {
        for (final addr in iface.addresses) {
          if (_isLanAddress(addr.address)) result.add(addr.address);
        }
      }
    } catch (_) {}
    return result;
  }

  bool _isLanAddress(String ip) {
    return ip.startsWith('192.168.') ||
        ip.startsWith('10.') ||
        ip.startsWith('172.');
  }

  Future<void> start() async {
    if (_server != null) return;
    final handler = const Pipeline().addHandler(_handleRequest);
    _server = await shelf_io.serve(handler, InternetAddress.anyIPv4, transferServerPort);
  }

  Future<void> stop() async {
    await _server?.close(force: true);
    _server = null;
  }

  Future<Response> _handleRequest(Request request) async {
    final segments = request.url.pathSegments;
    if (segments.length == 2 && segments[0] == 'download') {
      final className = segments[1];
      if (!classNames.contains(className)) {
        return Response.notFound('Unknown class: $className');
      }
      return _serveClassZip(className);
    }
    return _serveIndexPage();
  }

  Future<Response> _serveClassZip(String className) async {
    final dir = await _classDirectory(className);
    final files = dir.listSync().whereType<File>().toList();
    if (files.isEmpty) {
      return Response.notFound('No images for class: $className');
    }
    final zipBytes = _buildZip(files, className);
    return Response.ok(
      zipBytes,
      headers: {
        'Content-Type': 'application/zip',
        'Content-Disposition': 'attachment; filename="$className.zip"',
        'Content-Length': '${zipBytes.length}',
      },
    );
  }

  Response _serveIndexPage() {
    final links = classNames.map((c) =>
      '<li><a href="/download/$c">$c.zip</a></li>'
    ).join('\n');
    final html = '''<!DOCTYPE html>
<html><head><meta charset="utf-8">
<title>TinyRobot Dataset Transfer</title>
<style>body{font-family:monospace;background:#111;color:#eee;padding:32px;}
a{color:#64b5f6;}li{margin:8px 0;font-size:18px;}</style>
</head><body>
<h2>TinyRobot Dataset Transfer</h2>
<p>Click a class to download its images as ZIP:</p>
<ul>$links</ul>
</body></html>''';
    return Response.ok(html, headers: {'Content-Type': 'text/html'});
  }

  Future<Directory> _classDirectory(String className) async {
    final external = await getExternalStorageDirectory();
    return Directory(
      '${external!.path.split('Android').first}Pictures/$_datasetRoot/raw/$className',
    );
  }

  Uint8List _buildZip(List<File> files, String className) {
    final archive = Archive();
    for (final file in files) {
      final bytes = file.readAsBytesSync();
      archive.addFile(ArchiveFile(file.uri.pathSegments.last, bytes.length, bytes));
    }
    final encoded = ZipEncoder().encode(archive);
    return Uint8List.fromList(encoded!);
  }
}
