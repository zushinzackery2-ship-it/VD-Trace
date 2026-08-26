import 'package:flutter/material.dart';

import '../theme/app_palette.dart';

/// A single-line / multi-line text input bound to a profile string value.
///
/// When no external [controller] is supplied the widget owns one seeded from
/// [value] and keeps it in sync when [value] changes from outside (e.g. after a
/// profile reload), without stomping the user's in-progress edit.
class VdTextField extends StatefulWidget
{
  const VdTextField({
    super.key,
    required this.label,
    required this.value,
    required this.onChanged,
    this.controller,
    this.hint,
    this.icon,
    this.maxLines = 1,
    this.enabled = true,
  });

  final String label;
  final String value;
  final String? hint;
  final IconData? icon;
  final void Function(String value) onChanged;
  final TextEditingController? controller;
  final int maxLines;
  final bool enabled;

  @override
  State<VdTextField> createState() => _VdTextFieldState();
}

class _VdTextFieldState extends State<VdTextField>
{
  late final TextEditingController _controller;
  bool _ownsController = false;

  @override
  void initState()
  {
    super.initState();
    if (widget.controller != null)
    {
      _controller = widget.controller!;
    }
    else
    {
      _controller = TextEditingController(text: widget.value);
      _ownsController = true;
    }
  }

  @override
  void didUpdateWidget(covariant VdTextField oldWidget)
  {
    super.didUpdateWidget(oldWidget);
    if (_ownsController && widget.value != oldWidget.value && widget.value != _controller.text)
    {
      _controller.text = widget.value;
    }
  }

  @override
  void dispose()
  {
    if (_ownsController)
    {
      _controller.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context)
  {
    return Padding(
      padding: const EdgeInsets.only(bottom: VdTokens.gapSmall),
      child: TextField(
        controller: _controller,
        maxLines: widget.maxLines,
        enabled: widget.enabled,
        style: const TextStyle(fontSize: 13, color: VdColors.textStrong),
        decoration: InputDecoration(
          labelText: widget.label,
          hintText: widget.hint,
          prefixIcon: widget.icon == null ? null : Icon(widget.icon, size: 18),
        ),
        onChanged: widget.onChanged,
      ),
    );
  }
}

/// A boolean toggle row with a soft surface background.
class VdSwitch extends StatelessWidget
{
  const VdSwitch({super.key, required this.label, required this.value, required this.onChanged, this.subtitle, this.enabled = true});

  final String label;
  final String? subtitle;
  final bool value;
  final void Function(bool value) onChanged;
  final bool enabled;

  @override
  Widget build(BuildContext context)
  {
    final active = value && enabled;
    return AnimatedOpacity(
      duration: const Duration(milliseconds: 150),
      opacity: enabled ? 1 : 0.5,
      child: Container(
        margin: const EdgeInsets.only(bottom: VdTokens.gapSmall),
        decoration: BoxDecoration(
          color: active ? VdColors.accentStrong.withValues(alpha: 0.12) : VdColors.surfaceMuted,
          borderRadius: VdTokens.inputRadius,
          border: Border.all(color: active ? VdColors.accent.withValues(alpha: 0.55) : VdColors.line),
        ),
        child: Material(
          type: MaterialType.transparency,
          child: SwitchListTile(
            value: value,
            dense: true,
            contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 2),
            activeThumbColor: VdColors.accent,
            activeTrackColor: VdColors.accentStrong.withValues(alpha: 0.55),
            title: Text(label, style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w600, color: VdColors.textStrong)),
            subtitle: subtitle == null ? null : Text(subtitle!, style: const TextStyle(fontSize: 11.5, color: VdColors.textMuted)),
            onChanged: enabled ? onChanged : null,
          ),
        ),
      ),
    );
  }
}

/// A single-select dropdown bound to a string value.
class VdDropdown extends StatelessWidget
{
  const VdDropdown({super.key, required this.label, required this.value, required this.values, required this.onChanged, this.icon, this.enabled = true});

  final String label;
  final String value;
  final List<String> values;
  final IconData? icon;
  final void Function(String value) onChanged;
  final bool enabled;

  @override
  Widget build(BuildContext context)
  {
    final current = values.contains(value) ? value : values.first;
    return Opacity(
      opacity: enabled ? 1 : 0.5,
      child: Padding(
        padding: const EdgeInsets.only(bottom: VdTokens.gapSmall),
        child: DropdownButtonFormField<String>(
          initialValue: current,
          isDense: true,
          dropdownColor: VdColors.surfaceRaised,
          borderRadius: VdTokens.inputRadius,
          icon: const Icon(Icons.expand_more, color: VdColors.textMuted),
          style: const TextStyle(fontSize: 13, color: VdColors.textStrong),
          decoration: InputDecoration(
            labelText: label,
            prefixIcon: icon == null ? null : Icon(icon, size: 18),
          ),
          items: [for (final item in values) DropdownMenuItem(value: item, child: Text(item))],
          onChanged: enabled ? (selected) => selected == null ? null : onChanged(selected) : null,
        ),
      ),
    );
  }
}

/// Responsive two-column grid that collapses to a single column when narrow.
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
        final twoColumns = constraints.maxWidth > 720;
        final width = twoColumns ? (constraints.maxWidth - VdTokens.gapLarge) / 2 : constraints.maxWidth;
        return Wrap(
          spacing: VdTokens.gapLarge,
          runSpacing: 0,
          children: [for (final child in children) SizedBox(width: width, child: child)],
        );
      },
    );
  }
}
