import 'package:flutter/material.dart';

import '../theme/app_palette.dart';

/// A compact key/value status chip (PID, TRACE, WRITE, ...).
class StatChip extends StatelessWidget
{
  const StatChip({super.key, required this.label, required this.value, this.color = VdColors.accent, this.icon, this.pulse = false});

  final String label;
  final String value;
  final Color color;
  final IconData? icon;
  final bool pulse;

  @override
  Widget build(BuildContext context)
  {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.12),
        border: Border.all(color: color.withValues(alpha: 0.42)),
        borderRadius: VdTokens.pillRadius,
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (icon != null) ...[Icon(icon, size: 13, color: color), const SizedBox(width: 6)]
          else ...[_Dot(color: color, pulse: pulse), const SizedBox(width: 7)],
          Text(label, style: const TextStyle(color: VdColors.textMuted, fontSize: 10.5, fontWeight: FontWeight.w600, letterSpacing: 0.4)),
          const SizedBox(width: 7),
          Text(value, style: TextStyle(color: color, fontWeight: FontWeight.w700, fontSize: 12)),
        ],
      ),
    );
  }
}

class _Dot extends StatelessWidget
{
  const _Dot({required this.color, required this.pulse});

  final Color color;
  final bool pulse;

  @override
  Widget build(BuildContext context)
  {
    return Container(
      width: 8,
      height: 8,
      decoration: BoxDecoration(
        color: color,
        shape: BoxShape.circle,
        boxShadow: pulse ? [BoxShadow(color: color.withValues(alpha: 0.7), blurRadius: 7, spreadRadius: 1)] : null,
      ),
    );
  }
}
