import 'package:flutter/material.dart';

import '../theme/app_palette.dart';

/// Describes one navigable section in the left rail.
class NavSection
{
  const NavSection({required this.label, required this.caption, required this.icon});

  final String label;
  final String caption;
  final IconData icon;
}

/// Ordered list of the eight control sections. Index order is a stable contract
/// used by the home shell (e.g. the address→memory jump targets index 4).
const List<NavSection> vdNavSections = [
  NavSection(label: '核心', caption: 'Core', icon: Icons.tune),
  NavSection(label: '策略', caption: 'Policy', icon: Icons.toggle_on),
  NavSection(label: '过滤', caption: 'Depth', icon: Icons.account_tree),
  NavSection(label: '观测', caption: 'Observer', icon: Icons.radar),
  NavSection(label: '内存', caption: 'Memory', icon: Icons.grid_view),
  NavSection(label: '预览', caption: 'Trace', icon: Icons.subject),
  NavSection(label: '日志', caption: 'Log', icon: Icons.terminal),
  NavSection(label: '模块', caption: 'Modules', icon: Icons.view_list),
];

/// Vertical section navigation shown on the left edge of the shell.
class NavRail extends StatelessWidget
{
  const NavRail({super.key, required this.selectedIndex, required this.onSelected});

  final int selectedIndex;
  final ValueChanged<int> onSelected;

  @override
  Widget build(BuildContext context)
  {
    return Container(
      width: 168,
      decoration: const BoxDecoration(
        color: VdColors.canvasTop,
        border: Border(right: BorderSide(color: VdColors.lineSoft)),
      ),
      child: ListView(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 14),
        children: [
          const Padding(
            padding: EdgeInsets.only(left: 8, bottom: 8),
            child: Text('CONFIGURATION', style: TextStyle(fontSize: 10, fontWeight: FontWeight.w700, letterSpacing: 1.1, color: VdColors.textFaint)),
          ),
          for (var index = 0; index < vdNavSections.length; index++)
            _NavItem(
              section: vdNavSections[index],
              selected: index == selectedIndex,
              onTap: () => onSelected(index),
            ),
        ],
      ),
    );
  }
}

class _NavItem extends StatelessWidget
{
  const _NavItem({required this.section, required this.selected, required this.onTap});

  final NavSection section;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context)
  {
    return Padding(
      padding: const EdgeInsets.only(bottom: 4),
      child: Material(
        color: Colors.transparent,
        child: InkWell(
          borderRadius: VdTokens.inputRadius,
          onTap: onTap,
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 150),
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 10),
            decoration: BoxDecoration(
              color: selected ? VdColors.accentStrong.withValues(alpha: 0.16) : Colors.transparent,
              borderRadius: VdTokens.inputRadius,
              border: Border.all(color: selected ? VdColors.accent.withValues(alpha: 0.5) : Colors.transparent),
            ),
            child: Row(
              children: [
                Icon(section.icon, size: 18, color: selected ? VdColors.accent : VdColors.textMuted),
                const SizedBox(width: 11),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(section.label, style: TextStyle(fontSize: 13.5, fontWeight: FontWeight.w700, color: selected ? VdColors.textStrong : VdColors.textMuted)),
                      Text(section.caption, style: TextStyle(fontSize: 10, letterSpacing: 0.4, color: selected ? VdColors.accent.withValues(alpha: 0.85) : VdColors.textFaint)),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
