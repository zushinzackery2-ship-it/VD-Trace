import 'package:flutter/material.dart';

const Color vdBg = Color(0xfff5f8fc);
const Color vdPanel = Color(0xffffffff);
const Color vdPanelSoft = Color(0xffeef4fb);
const Color vdLine = Color(0xffd6e0eb);
const Color vdBlue = Color(0xff2563eb);
const Color vdCyan = Color(0xff0891b2);
const Color vdAmber = Color(0xffd97706);
const Color vdTextMuted = Color(0xff64748b);
const Color vdTextStrong = Color(0xff0f172a);

ThemeData buildVdTraceTheme()
{
  final scheme = ColorScheme.fromSeed(seedColor: vdBlue, brightness: Brightness.light).copyWith(
    primary: vdBlue,
    secondary: vdCyan,
    surface: vdPanel,
  );
  return ThemeData(
    colorScheme: scheme,
    scaffoldBackgroundColor: vdBg,
    useMaterial3: true,
    fontFamily: 'Segoe UI',
    visualDensity: VisualDensity.compact,
    cardTheme: CardThemeData(
      color: vdPanel,
      elevation: 1,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16), side: const BorderSide(color: vdLine)),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: const Color(0xfff8fbff),
      isDense: true,
      contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      border: OutlineInputBorder(borderRadius: BorderRadius.circular(12), borderSide: const BorderSide(color: vdLine)),
      enabledBorder: OutlineInputBorder(borderRadius: BorderRadius.circular(12), borderSide: const BorderSide(color: vdLine)),
      focusedBorder: OutlineInputBorder(borderRadius: BorderRadius.circular(12), borderSide: const BorderSide(color: vdBlue, width: 1.4)),
      labelStyle: const TextStyle(color: vdTextMuted),
    ),
    tabBarTheme: const TabBarThemeData(
      dividerColor: Colors.transparent,
      labelColor: vdTextStrong,
      unselectedLabelColor: vdTextMuted,
      indicatorColor: vdCyan,
      indicatorSize: TabBarIndicatorSize.label,
    ),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        backgroundColor: vdBlue,
        foregroundColor: Colors.white,
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 11),
        minimumSize: const Size(0, 40),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      ),
    ),
  );
}
