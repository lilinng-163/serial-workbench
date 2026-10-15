// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "controls.h"

namespace sw
{
void showSshAuthPrompt(SshAuthPrompt *prompt, QWidget *parent, const Theme &theme);
void clearSshCache(const Config &config, Engine *engine, QWidget *parent, const Theme &theme);
}
