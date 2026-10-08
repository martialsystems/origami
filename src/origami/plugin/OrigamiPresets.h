// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

// ORIGAMI factory presets. The bank is presets/factory.xml, compiled into the plugin as binary data
// (OrigamiFactoryData), so it needs no files on disk on any platform. Each <PRESET name bank> holds one complete
// ORIGAMI state (format 1, the same XML the plugin saves), read with the same stateFromXml as a saved state.
// INIT is always first and equals the parameter defaults. Banks, in order: RONIN (tuned for RONIN's VCO and VCF
// levels), SHOGUN (drums), BUSHIDO (folds meant to be CV-sequenced on VC 1-3) and GENERIC (any source).
// The host sees the bank as the plugin's program list ("RONIN: Acid Grit"); the panel's preset box groups by bank.

#include "origami/dsp/OrigamiParams.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace origami {

struct FactoryPreset
{
    juce::String name, bank;
    double values[kParamCount] {};
    // "INIT", or "BANK: Name" (the host's program list).
    juce::String displayName() const { return bank.isEmpty() || bank == "INIT" ? name : bank + ": " + name; }
};

// Parses a bank. Presets that fail to load are skipped and reported in errors (when given).
std::vector<FactoryPreset> parseFactoryBank (const juce::XmlElement& bank, juce::StringArray* errors = nullptr);

// The embedded factory bank, parsed once.
const std::vector<FactoryPreset>& factoryPresets();

// The embedded factory.xml as text (tests).
juce::String factoryBankXmlText();

} // namespace origami
