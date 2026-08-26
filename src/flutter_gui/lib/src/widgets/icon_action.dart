import 'package:flutter/material.dart';

import '../theme/app_palette.dart';

/// Compact icon-only action used inside console panel headers.
class IconAction extends StatelessWidget
{
  const IconAction({super.key, required this.icon, required this.tooltip, required this.onPressed});

  final IconData icon;
  final String tooltip;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context)
  {
    final enabled = onPressed != null;
    return Tooltip(
      message: tooltip,
      child: InkWell(
        borderRadius: BorderRadius.circular(8),
        onTap: onPressed,
        child: Container(
          width: 32,
          height: 30,
          decoration: BoxDecoration(
            color: VdColors.surfaceMuted,
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: enabled ? VdColors.line : VdColors.lineSoft),
          ),
          child: Icon(icon, size: 15, color: enabled ? VdColors.textStrong : VdColors.textFaint),
        ),
      ),
    );
  }
}
