import 'package:flutter/material.dart';

class ZoneOverlayPainter extends CustomPainter {
  static const List<double> _dashArray = [6.0, 4.0];
  
  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = Colors.white.withOpacity(0.5)
      ..strokeWidth = 1.5
      ..style = PaintingStyle.stroke;
    
    // Left vertical line at 30%
    final leftX = size.width * 0.3;
    _drawDashedLine(canvas, Offset(leftX, 0), Offset(leftX, size.height), paint);
    
    // Right vertical line at 70%
    final rightX = size.width * 0.7;
    _drawDashedLine(canvas, Offset(rightX, 0), Offset(rightX, size.height), paint);
    
    // Zone labels
    final labelPaint = TextPainter(
      text: TextSpan(
        text: 'L',
        style: TextStyle(
          color: Colors.white.withOpacity(0.4),
          fontSize: 11,
          fontWeight: FontWeight.bold,
        ),
      ),
      textDirection: TextDirection.ltr,
    );
    
    // Left label
    labelPaint.layout();
    labelPaint.paint(
      canvas,
      Offset(leftX / 2 - labelPaint.width / 2, size.height / 2 - labelPaint.height / 2),
    );
    
    // Center label
    labelPaint.text = TextSpan(
      text: 'FWD',
      style: const TextStyle(
        color: Colors.white,
        fontSize: 11,
        fontWeight: FontWeight.bold,
      ),
    );
    labelPaint.layout();
    labelPaint.paint(
      canvas,
      Offset(size.width / 2 - labelPaint.width / 2, size.height / 2 - labelPaint.height / 2),
    );
    
    // Right label
    labelPaint.text = TextSpan(
      text: 'R',
      style: TextStyle(
        color: Colors.white.withOpacity(0.4),
        fontSize: 11,
        fontWeight: FontWeight.bold,
      ),
    );
    labelPaint.layout();
    labelPaint.paint(
      canvas,
      Offset((leftX + rightX) / 2 - labelPaint.width / 2, size.height / 2 - labelPaint.height / 2),
    );
  }
  
  void _drawDashedLine(Canvas canvas, Offset start, Offset end, Paint paint) {
    final path = Path();
    final distance = (end - start).distance;
    final dashLength = _dashArray[0];
    final gapLength = _dashArray[1];
    final totalDashLength = dashLength + gapLength;
    
    double currentDistance = 0;
    while (currentDistance < distance) {
      final segmentStart = start + (end - start) * (currentDistance / distance);
      final segmentEnd = start + (end - start) * ((currentDistance + dashLength).clamp(0.0, distance) / distance);
      path.moveTo(segmentStart.dx, segmentStart.dy);
      path.lineTo(segmentEnd.dx, segmentEnd.dy);
      currentDistance += totalDashLength;
    }
    
    canvas.drawPath(path, paint);
  }
  
  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}
