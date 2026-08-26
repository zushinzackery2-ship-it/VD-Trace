import 'package:flutter/material.dart';

import '../theme/app_palette.dart';

/// A monospaced, selectable output surface for logs, previews and results.
class ConsolePanel extends StatelessWidget
{
  const ConsolePanel({
    super.key,
    required this.title,
    required this.text,
    this.subtitle,
    this.minHeight = 180,
    this.actions = const [],
    this.accent = VdColors.cyan,
    this.emptyHint = '暂无输出。',
  });

  final String title;
  final String? subtitle;
  final String text;
  final double minHeight;
  final List<Widget> actions;
  final Color accent;
  final String emptyHint;

  @override
  Widget build(BuildContext context)
  {
    final isEmpty = text.trim().isEmpty;
    return Container(
      margin: const EdgeInsets.only(bottom: VdTokens.gap),
      decoration: BoxDecoration(
        color: VdColors.surface,
        borderRadius: VdTokens.cardRadius,
        border: Border.all(color: VdColors.line),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 14, 12, 10),
            child: Row(
              children: [
                Container(width: 3, height: 16, decoration: BoxDecoration(color: accent, borderRadius: BorderRadius.circular(9))),
                const SizedBox(width: 10),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(title, style: Theme.of(context).textTheme.titleSmall),
                      if (subtitle != null && subtitle!.isNotEmpty)
                        Padding(
                          padding: const EdgeInsets.only(top: 2),
                          child: Text(subtitle!, maxLines: 1, overflow: TextOverflow.ellipsis, style: const TextStyle(color: VdColors.textMuted, fontSize: 11.5)),
                        ),
                    ],
                  ),
                ),
                if (actions.isNotEmpty) Wrap(spacing: VdTokens.gapSmall, children: actions),
              ],
            ),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(12, 0, 12, 12),
            child: Container(
              constraints: BoxConstraints(minHeight: minHeight),
              width: double.infinity,
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: VdColors.mono,
                borderRadius: VdTokens.inputRadius,
                border: Border.all(color: VdColors.lineSoft),
              ),
              child: SelectableText(
                isEmpty ? emptyHint : text,
                style: TextStyle(
                  fontFamily: VdTokens.monoFamily,
                  fontSize: 12,
                  height: 1.42,
                  color: isEmpty ? VdColors.textFaint : VdColors.textStrong,
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
