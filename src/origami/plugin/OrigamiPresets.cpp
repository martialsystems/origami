// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "OrigamiPresets.h"
#include "OrigamiState.h"

#include "OrigamiFactoryData.h"

namespace origami {

std::vector<FactoryPreset> parseFactoryBank (const juce::XmlElement& bank, juce::StringArray* errors)
{
    std::vector<FactoryPreset> out;
    if (! bank.hasTagName ("ORIGAMI_FACTORY"))
    {
        if (errors != nullptr) errors->add ("not an ORIGAMI factory bank");
        return out;
    }
    for (auto* e : bank.getChildWithTagNameIterator ("PRESET"))
    {
        FactoryPreset p;
        p.name = e->getStringAttribute ("name").trim();
        p.bank = e->getStringAttribute ("bank");
        const auto* state = e->getChildByName (kStateUnit);
        const auto load = state != nullptr ? stateFromXml (*state, p.values) : StateLoad { false, "no ORIGAMI state" };
        if (p.name.isEmpty() || ! load.ok)
        {
            if (errors != nullptr) errors->add ("preset \"" + p.name + "\": " + (p.name.isEmpty() ? juce::String ("no name") : load.message));
            continue;
        }
        out.push_back (p);
    }
    return out;
}

juce::String factoryBankXmlText()
{
    return juce::String::fromUTF8 (OrigamiFactoryData::factory_xml, OrigamiFactoryData::factory_xmlSize);
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> bank = []
    {
        std::vector<FactoryPreset> b;
        if (auto xml = juce::parseXML (factoryBankXmlText()))
            b = parseFactoryBank (*xml);
        if (b.empty())
        {
            FactoryPreset init;
            init.name = "INIT";
            init.bank = "INIT";
            for (int p = 0; p < kParamCount; ++p) init.values[p] = paramInfo (p).def;
            b.push_back (init);
        }
        return b;
    }();
    return bank;
}

} // namespace origami
