// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "background_image.h"
#include "controls.h"

namespace sw
{
// Install before creating the first window; dialogs created later are included.
void installWindowChrome();
void updateWindowChrome(const Theme &theme, const BackgroundSpec &spec, const QImage &image);
}
