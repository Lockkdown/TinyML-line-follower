import 'package:flutter/material.dart';

class ControlButton extends StatefulWidget {
  final Widget child;
  final VoidCallback? onPressed;
  final VoidCallback? onReleased;
  final VoidCallback? onCanceled;
  final Color backgroundColor;
  final double size;

  const ControlButton({
    super.key,
    required this.child,
    this.onPressed,
    this.onReleased,
    this.onCanceled,
    required this.backgroundColor,
    this.size = 80,
  });

  @override
  State<ControlButton> createState() => _ControlButtonState();
}

class _ControlButtonState extends State<ControlButton>
    with SingleTickerProviderStateMixin {
  late AnimationController _controller;
  late Animation<double> _scaleAnimation;
  bool _pressed = false;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      duration: const Duration(milliseconds: 100),
      vsync: this,
    );
    _scaleAnimation = Tween<double>(begin: 1.0, end: 0.95).animate(
      CurvedAnimation(parent: _controller, curve: Curves.easeInOut),
    );
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _handlePress() {
    if (!_pressed) {
      _pressed = true;
      _controller.forward();
      widget.onPressed?.call();
    }
  }

  void _handleRelease() {
    if (_pressed) {
      _pressed = false;
      _controller.reverse();
      widget.onReleased?.call();
    }
  }

  void _handleCancel() {
    if (_pressed) {
      _pressed = false;
      _controller.reverse();
      widget.onCanceled?.call();
    }
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) => _handlePress(),
      onTapUp: (_) => _handleRelease(),
      onTapCancel: _handleCancel,
      child: AnimatedBuilder(
        animation: _scaleAnimation,
        builder: (context, child) {
          return Transform.scale(
            scale: _scaleAnimation.value,
            child: Container(
              width: widget.size,
              height: widget.size,
              decoration: BoxDecoration(
                color: widget.backgroundColor,
                borderRadius: BorderRadius.circular(12),
                boxShadow: const [
                  BoxShadow(
                    color: Colors.black26,
                    offset: Offset(0, 4),
                    blurRadius: 6,
                  ),
                ],
              ),
              child: Center(child: widget.child),
            ),
          );
        },
      ),
    );
  }
}
