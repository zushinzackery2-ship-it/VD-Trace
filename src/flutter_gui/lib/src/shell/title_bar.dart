import 'package:flutter/material.dart';

import '../theme/app_palette.dart';
import '../window_control.dart';

/// Frameless-window title bar: brand mark, draggable region and caption buttons.
class TitleBar extends StatelessWidget
{
  const TitleBar({super.key});

  @override
  Widget build(BuildContext context)
  {
    return Container(
      height: 46,
      padding: const EdgeInsets.only(left: 16, right: 8),
      decoration: const BoxDecoration(
        border: Border(bottom: BorderSide(color: VdColors.lineSoft)),
      ),
      child: Row(
        children: [
          const _BrandMark(),
          const SizedBox(width: 14),
          Expanded(
            child: GestureDetector(
              behavior: HitTestBehavior.opaque,
              onPanDown: (_) => startWindowDrag(),
              onDoubleTap: maximizeOrRestoreWindow,
              child: const SizedBox(height: 46),
            ),
          ),
          const _CaptionButtons(),
        ],
      ),
    );
  }
}

class _BrandMark extends StatelessWidget
{
  const _BrandMark();

  @override
  Widget build(BuildContext context)
  {
    return Row(
      key: const ValueKey('vdtrace_brand_title'),
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          width: 30,
          height: 30,
          decoration: BoxDecoration(
            gradient: const LinearGradient(
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
              colors: [VdColors.accent, VdColors.cyan],
            ),
            borderRadius: BorderRadius.circular(9),
            boxShadow: [BoxShadow(color: VdColors.accent.withValues(alpha: 0.4), blurRadius: 12, offset: const Offset(0, 3))],
          ),
          child: const Icon(Icons.radar, size: 18, color: Colors.white),
        ),
        const SizedBox(width: 10),
        const Text('VD-Trace', style: TextStyle(fontSize: 17, fontWeight: FontWeight.w800, letterSpacing: 0.2, color: VdColors.textStrong)),
        const SizedBox(width: 8),
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 2),
          decoration: BoxDecoration(
            color: VdColors.surfaceRaised,
            borderRadius: BorderRadius.circular(6),
            border: Border.all(color: VdColors.line),
          ),
          child: const Text('CONTROL', style: TextStyle(fontSize: 9.5, fontWeight: FontWeight.w700, letterSpacing: 1.2, color: VdColors.textMuted)),
        ),
      ],
    );
  }
}

class _CaptionButtons extends StatelessWidget
{
  const _CaptionButtons();

  @override
  Widget build(BuildContext context)
  {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        _CaptionButton(icon: Icons.remove, tooltip: '最小化', onPressed: minimizeWindow),
        _CaptionButton(icon: Icons.crop_square, tooltip: '最大化/还原', onPressed: maximizeOrRestoreWindow),
        _CaptionButton(icon: Icons.close, tooltip: '关闭', danger: true, onPressed: closeWindow),
      ],
    );
  }
}

class _CaptionButton extends StatelessWidget
{
  const _CaptionButton({required this.icon, required this.tooltip, required this.onPressed, this.danger = false});

  final IconData icon;
  final String tooltip;
  final VoidCallback onPressed;
  final bool danger;

  @override
  Widget build(BuildContext context)
  {
    return Tooltip(
      message: tooltip,
      child: InkWell(
        borderRadius: BorderRadius.circular(8),
        hoverColor: danger ? VdColors.danger.withValues(alpha: 0.18) : VdColors.surfaceRaised,
        onTap: onPressed,
        child: SizedBox(
          width: 42,
          height: 34,
          child: Icon(icon, size: 15, color: danger ? VdColors.danger : VdColors.textMuted),
        ),
      ),
    );
  }
}
