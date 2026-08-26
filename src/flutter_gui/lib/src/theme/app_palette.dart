import 'package:flutter/material.dart';

/// Central design tokens for the VD-Trace control surface.
///
/// The palette targets a calm, high-contrast "diagnostics console" look: a deep
/// slate canvas, softly elevated panels, and two cool accents (indigo + cyan)
/// with clear semantic colors for run/write/error state.
class VdColors
{
  const VdColors._();

  static const Color canvasTop = Color(0xff0b1120);
  static const Color canvasBottom = Color(0xff0d1526);

  static const Color surface = Color(0xff141d31);
  static const Color surfaceMuted = Color(0xff18223a);
  static const Color surfaceRaised = Color(0xff1c2942);
  static const Color surfaceInput = Color(0xff0f182a);
  static const Color mono = Color(0xff0a1120);

  static const Color line = Color(0xff273350);
  static const Color lineSoft = Color(0xff1f2a44);

  static const Color accent = Color(0xff6d8bff);
  static const Color accentStrong = Color(0xff4f6bff);
  static const Color cyan = Color(0xff34d3ee);
  static const Color violet = Color(0xffa78bfa);

  static const Color success = Color(0xff34d399);
  static const Color warning = Color(0xfffbbf24);
  static const Color danger = Color(0xfff87171);

  static const Color textStrong = Color(0xffe8eefc);
  static const Color textMuted = Color(0xff97a6c4);
  static const Color textFaint = Color(0xff62729a);
}

/// Shape, spacing and typography tokens shared across the UI.
class VdTokens
{
  const VdTokens._();

  static const double gap = 12;
  static const double gapSmall = 8;
  static const double gapLarge = 16;

  static const double radius = 14;
  static const double radiusSmall = 10;
  static const double radiusPill = 999;

  static const String monoFamily = 'Consolas';
  static const String uiFamily = 'Segoe UI';

  static BorderRadius get cardRadius => BorderRadius.circular(radius);
  static BorderRadius get inputRadius => BorderRadius.circular(radiusSmall);
  static BorderRadius get pillRadius => BorderRadius.circular(radiusPill);
}

/// Full-canvas background gradient used by the shell.
const LinearGradient vdCanvasGradient = LinearGradient(
  begin: Alignment.topLeft,
  end: Alignment.bottomRight,
  colors: [VdColors.canvasTop, VdColors.canvasBottom],
);
