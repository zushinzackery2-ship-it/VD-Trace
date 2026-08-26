import 'package:flutter/material.dart';

import '../theme/app_palette.dart';

/// A titled surface panel used to group related controls on a page.
class SectionCard extends StatelessWidget
{
  const SectionCard({
    super.key,
    required this.title,
    required this.child,
    this.subtitle,
    this.icon,
    this.accent = VdColors.cyan,
    this.trailing,
  });

  final String title;
  final String? subtitle;
  final IconData? icon;
  final Color accent;
  final Widget? trailing;
  final Widget child;

  @override
  Widget build(BuildContext context)
  {
    return Container(
      margin: const EdgeInsets.only(bottom: VdTokens.gap),
      decoration: BoxDecoration(
        color: VdColors.surface,
        borderRadius: VdTokens.cardRadius,
        border: Border.all(color: VdColors.line),
        boxShadow: [
          BoxShadow(color: Colors.black.withValues(alpha: 0.25), blurRadius: 22, offset: const Offset(0, 10)),
        ],
      ),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            _Header(title: title, subtitle: subtitle, icon: icon, accent: accent, trailing: trailing),
            const SizedBox(height: VdTokens.gap),
            child,
          ],
        ),
      ),
    );
  }
}

class _Header extends StatelessWidget
{
  const _Header({required this.title, required this.subtitle, required this.icon, required this.accent, required this.trailing});

  final String title;
  final String? subtitle;
  final IconData? icon;
  final Color accent;
  final Widget? trailing;

  @override
  Widget build(BuildContext context)
  {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          width: 34,
          height: 34,
          decoration: BoxDecoration(
            gradient: LinearGradient(
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
              colors: [accent.withValues(alpha: 0.28), accent.withValues(alpha: 0.08)],
            ),
            borderRadius: BorderRadius.circular(VdTokens.radiusSmall),
            border: Border.all(color: accent.withValues(alpha: 0.45)),
          ),
          child: Icon(icon ?? Icons.tune, size: 18, color: accent),
        ),
        const SizedBox(width: VdTokens.gap),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(title, style: Theme.of(context).textTheme.titleMedium),
              if (subtitle != null && subtitle!.isNotEmpty) ...[
                const SizedBox(height: 3),
                Text(subtitle!, style: const TextStyle(color: VdColors.textMuted, fontSize: 12, height: 1.3)),
              ],
            ],
          ),
        ),
        if (trailing != null) ...[const SizedBox(width: VdTokens.gap), trailing!],
      ],
    );
  }
}

/// Vertical padded scroll body for a page of section cards.
class PageBody extends StatelessWidget
{
  const PageBody({super.key, required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context)
  {
    return ListView(
      padding: const EdgeInsets.fromLTRB(18, 18, 18, 24),
      children: children,
    );
  }
}
