# Serial Workbench icon

Original project artwork, Apache-2.0.

`lightning.svg` is the source: a light-green double-fold lightning motif rotated
45 degrees clockwise, with a transparent background. `lightning.ico` contains
16, 20, 24, 32, 40, 48, 64, 128 and 256 pixel images, all with alpha transparency.

Run `scripts/generate-icon.py` with the build environment's PySide6 to regenerate
the ICO, PNG preview and installer bitmaps. The installer bitmap background is
wizard chrome; the application, setup and uninstall icon itself is transparent.
No Python dependency is introduced in the deployed application.
