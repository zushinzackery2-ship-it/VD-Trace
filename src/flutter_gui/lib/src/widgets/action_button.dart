import 'package:flutter/material.dart';

import '../theme/app_palette.dart';

/// Primary filled action button with a leading icon.
class ActionButton extends StatelessWidget
{
  const ActionButton({super.key, required this.label, required this.onPressed, this.icon, this.tone = ActionTone.primary});

  final String label;
  final IconData? icon;
  final VoidCallback? onPressed;
  final ActionTone tone;

  @override
  Widget build(BuildContext context)
  {
    final colors = _toneColors(tone);
    return FilledButton.icon(
      onPressed: onPressed,
      icon: Icon(icon ?? Icons.bolt, size: 18),
      label: Text(label),
      style: FilledButton.styleFrom(
        backgroundColor: colors.$1,
        foregroundColor: colors.$2,
        disabledBackgroundColor: VdColors.surfaceRaised,
        disabledForegroundColor: VdColors.textFaint,
      ),
    );
  }

  (Color, Color) _toneColors(ActionTone tone)
  {
    switch (tone)
    {
      case ActionTone.primary:
        return (VdColors.accentStrong, Colors.white);
      case ActionTone.positive:
        return (VdColors.success.withValues(alpha: 0.92), const Color(0xff05231a));
      case ActionTone.danger:
        return (VdColors.danger.withValues(alpha: 0.92), const Color(0xff2a0b0b));
    }
  }
}

enum ActionTone { primary, positive, danger }

/// Low-emphasis secondary button used for utility actions inside panels.
class GhostButton extends StatelessWidget
{
  const GhostButton({super.key, required this.label, required this.onPressed, this.icon});

  final String label;
  final IconData? icon;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context)
  {
    final enabled = onPressed != null;
    return OutlinedButton.icon(
      onPressed: onPressed,
      icon: Icon(icon ?? Icons.chevron_right, size: 17),
      label: Text(label),
      style: OutlinedButton.styleFrom(
        foregroundColor: enabled ? VdColors.textStrong : VdColors.textFaint,
        backgroundColor: VdColors.surfaceMuted,
        side: BorderSide(color: enabled ? VdColors.line : VdColors.lineSoft),
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 11),
        minimumSize: const Size(0, 40),
        textStyle: const TextStyle(fontWeight: FontWeight.w600, fontSize: 12.5),
        shape: RoundedRectangleBorder(borderRadius: VdTokens.inputRadius),
      ),
    );
  }
}
