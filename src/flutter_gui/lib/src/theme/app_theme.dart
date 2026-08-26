import 'package:flutter/material.dart';

import 'app_palette.dart';

/// Builds the Material 3 dark theme for the VD-Trace GUI.
ThemeData buildVdTraceTheme()
{
  final scheme = ColorScheme.fromSeed(
    seedColor: VdColors.accentStrong,
    brightness: Brightness.dark,
  ).copyWith(
    primary: VdColors.accent,
    secondary: VdColors.cyan,
    surface: VdColors.surface,
    error: VdColors.danger,
    onSurface: VdColors.textStrong,
  );

  return ThemeData(
    colorScheme: scheme,
    scaffoldBackgroundColor: VdColors.canvasBottom,
    useMaterial3: true,
    fontFamily: VdTokens.uiFamily,
    visualDensity: VisualDensity.compact,
    splashFactory: InkSparkle.splashFactory,
    textTheme: _textTheme,
    cardTheme: CardThemeData(
      color: VdColors.surface,
      elevation: 0,
      margin: EdgeInsets.zero,
      shape: RoundedRectangleBorder(
        borderRadius: VdTokens.cardRadius,
        side: const BorderSide(color: VdColors.line),
      ),
    ),
    dividerTheme: const DividerThemeData(color: VdColors.lineSoft, thickness: 1, space: 1),
    inputDecorationTheme: _inputTheme,
    filledButtonTheme: _filledButtonTheme,
    tooltipTheme: _tooltipTheme,
    snackBarTheme: const SnackBarThemeData(
      behavior: SnackBarBehavior.floating,
      backgroundColor: VdColors.surfaceRaised,
      contentTextStyle: TextStyle(color: VdColors.textStrong),
    ),
    scrollbarTheme: ScrollbarThemeData(
      thumbColor: WidgetStatePropertyAll(VdColors.line.withValues(alpha: 0.9)),
      radius: const Radius.circular(VdTokens.radiusPill),
      thickness: const WidgetStatePropertyAll(6),
    ),
  );
}

const TextTheme _textTheme = TextTheme(
  titleMedium: TextStyle(color: VdColors.textStrong, fontWeight: FontWeight.w700, fontSize: 15),
  titleSmall: TextStyle(color: VdColors.textStrong, fontWeight: FontWeight.w600, fontSize: 13),
  bodyMedium: TextStyle(color: VdColors.textStrong, fontSize: 13),
  bodySmall: TextStyle(color: VdColors.textMuted, fontSize: 12),
  labelLarge: TextStyle(color: VdColors.textStrong, fontWeight: FontWeight.w600),
);

final InputDecorationTheme _inputTheme = InputDecorationTheme(
  filled: true,
  fillColor: VdColors.surfaceInput,
  isDense: true,
  contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
  labelStyle: const TextStyle(color: VdColors.textMuted, fontSize: 12.5),
  floatingLabelStyle: const TextStyle(color: VdColors.accent, fontSize: 12.5),
  hintStyle: const TextStyle(color: VdColors.textFaint),
  prefixIconColor: VdColors.textMuted,
  border: OutlineInputBorder(borderRadius: VdTokens.inputRadius, borderSide: const BorderSide(color: VdColors.line)),
  enabledBorder: OutlineInputBorder(borderRadius: VdTokens.inputRadius, borderSide: const BorderSide(color: VdColors.line)),
  disabledBorder: OutlineInputBorder(borderRadius: VdTokens.inputRadius, borderSide: const BorderSide(color: VdColors.lineSoft)),
  focusedBorder: OutlineInputBorder(borderRadius: VdTokens.inputRadius, borderSide: const BorderSide(color: VdColors.accent, width: 1.5)),
);

final FilledButtonThemeData _filledButtonTheme = FilledButtonThemeData(
  style: FilledButton.styleFrom(
    backgroundColor: VdColors.accentStrong,
    foregroundColor: Colors.white,
    disabledBackgroundColor: VdColors.surfaceRaised,
    disabledForegroundColor: VdColors.textFaint,
    padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
    minimumSize: const Size(0, 42),
    textStyle: const TextStyle(fontWeight: FontWeight.w600, fontSize: 13),
    shape: RoundedRectangleBorder(borderRadius: VdTokens.inputRadius),
  ),
);

const TooltipThemeData _tooltipTheme = TooltipThemeData(
  decoration: BoxDecoration(
    color: VdColors.surfaceRaised,
    borderRadius: BorderRadius.all(Radius.circular(VdTokens.radiusSmall)),
    border: Border.fromBorderSide(BorderSide(color: VdColors.line)),
  ),
  textStyle: TextStyle(color: VdColors.textStrong, fontSize: 12),
  waitDuration: Duration(milliseconds: 400),
);
