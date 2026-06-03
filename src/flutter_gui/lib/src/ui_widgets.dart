import 'package:flutter/material.dart';

import 'ui_theme.dart';

class PagePad extends StatelessWidget
{
  const PagePad({super.key, required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context)
  {
    return ListView(padding: const EdgeInsets.all(16), children: children);
  }
}

class SectionCard extends StatelessWidget
{
  const SectionCard({super.key, required this.title, required this.child, this.subtitle});

  final String title;
  final String? subtitle;
  final Widget child;

  @override
  Widget build(BuildContext context)
  {
    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Container(width: 4, height: 20, decoration: BoxDecoration(color: vdCyan, borderRadius: BorderRadius.circular(99))),
                const SizedBox(width: 10),
                Text(title, style: Theme.of(context).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.w800)),
              ],
            ),
            if (subtitle != null) ...[
              const SizedBox(height: 4),
              Text(subtitle!, style: const TextStyle(color: vdTextMuted, fontSize: 12)),
            ],
            const SizedBox(height: 12),
            child,
          ],
        ),
      ),
    );
  }
}

class StatPill extends StatelessWidget
{
  const StatPill({super.key, required this.label, required this.value, this.color = vdBlue});

  final String label;
  final String value;
  final Color color;

  @override
  Widget build(BuildContext context)
  {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 11, vertical: 7),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.12),
        border: Border.all(color: color.withValues(alpha: 0.45)),
        borderRadius: BorderRadius.circular(999),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(label, style: const TextStyle(color: vdTextMuted, fontSize: 11)),
          const SizedBox(width: 8),
          Text(value, style: TextStyle(color: color, fontWeight: FontWeight.w800, fontSize: 12)),
        ],
      ),
    );
  }
}

class TextFieldRow extends StatefulWidget
{
  const TextFieldRow({super.key, required this.label, required this.value, required this.onChanged, this.controller, this.maxLines = 1, this.enabled = true});

  final String label;
  final String value;
  final void Function(String value) onChanged;
  final TextEditingController? controller;
  final int maxLines;
  final bool enabled;

  @override
  State<TextFieldRow> createState() => _TextFieldRowState();
}

class _TextFieldRowState extends State<TextFieldRow>
{
  late final TextEditingController controller;
  bool _ownsController = false;

  @override
  void initState()
  {
    super.initState();
    if (widget.controller != null)
    {
      controller = widget.controller!;
    }
    else
    {
      controller = TextEditingController(text: widget.value);
      _ownsController = true;
    }
  }

  @override
  void didUpdateWidget(covariant TextFieldRow oldWidget)
  {
    super.didUpdateWidget(oldWidget);
    if (_ownsController && widget.value != oldWidget.value && widget.value != controller.text)
    {
      controller.text = widget.value;
    }
  }

  @override
  void dispose()
  {
    if (_ownsController)
    {
      controller.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context)
  {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: TextField(
        controller: controller,
        maxLines: widget.maxLines,
        enabled: widget.enabled,
        decoration: InputDecoration(labelText: widget.label),
        onChanged: widget.onChanged,
      ),
    );
  }
}

class SwitchRow extends StatelessWidget
{
  const SwitchRow({super.key, required this.label, required this.value, required this.onChanged, this.enabled = true});

  final String label;
  final bool value;
  final void Function(bool value) onChanged;
  final bool enabled;

  @override
  Widget build(BuildContext context)
  {
    return Opacity(
      opacity: enabled ? 1.0 : 0.45,
      child: Container(
        margin: const EdgeInsets.only(bottom: 6),
        decoration: BoxDecoration(color: vdPanelSoft, borderRadius: BorderRadius.circular(12), border: Border.all(color: vdLine)),
        child: SwitchListTile(value: value, title: Text(label), onChanged: enabled ? onChanged : null),
      ),
    );
  }
}

class DropdownRow extends StatelessWidget
{
  const DropdownRow({super.key, required this.label, required this.value, required this.values, required this.onChanged, this.enabled = true});

  final String label;
  final String value;
  final List<String> values;
  final void Function(String value) onChanged;
  final bool enabled;

  @override
  Widget build(BuildContext context)
  {
    final current = values.contains(value) ? value : values.first;
    return Opacity(
      opacity: enabled ? 1.0 : 0.45,
      child: Padding(
        padding: const EdgeInsets.only(bottom: 8),
        child: DropdownButtonFormField<String>(
          initialValue: current,
          decoration: InputDecoration(labelText: label),
          items: [for (final item in values) DropdownMenuItem(value: item, child: Text(item))],
          onChanged: enabled ? (value) => value == null ? null : onChanged(value) : null,
        ),
      ),
    );
  }
}

class ActionButton extends StatelessWidget
{
  const ActionButton({super.key, required this.label, required this.onPressed, this.icon});

  final String label;
  final IconData? icon;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context)
  {
    return FilledButton.icon(onPressed: onPressed, icon: Icon(icon ?? Icons.bolt, size: 18), label: Text(label));
  }
}

class TextPanel extends StatelessWidget
{
  const TextPanel({super.key, required this.title, required this.text, this.minHeight = 180, this.actions = const []});

  final String title;
  final String text;
  final double minHeight;
  final List<Widget> actions;

  @override
  Widget build(BuildContext context)
  {
    return SectionCard(
      title: title,
      subtitle: actions.isEmpty ? null : null,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (actions.isNotEmpty) Padding(
            padding: const EdgeInsets.only(bottom: 8),
            child: Wrap(spacing: 8, children: actions),
          ),
          Container(
            constraints: BoxConstraints(minHeight: minHeight),
            width: double.infinity,
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: const Color(0xfff8fbff),
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: vdLine),
              boxShadow: [BoxShadow(color: vdBlue.withValues(alpha: 0.06), blurRadius: 18)],
            ),
            child: SelectableText(text.isEmpty ? ' ' : text, style: const TextStyle(fontFamily: 'Consolas', fontSize: 12, height: 1.28, color: vdTextStrong)),
          ),
        ],
      ),
    );
  }
}

class FieldGrid extends StatelessWidget
{
  const FieldGrid({super.key, required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context)
  {
    return LayoutBuilder(
      builder: (context, constraints)
      {
        final width = constraints.maxWidth > 900 ? (constraints.maxWidth - 14) / 2 : constraints.maxWidth;
        return Wrap(spacing: 14, runSpacing: 2, children: [for (final child in children) SizedBox(width: width, child: child)]);
      },
    );
  }
}
