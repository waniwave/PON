#include "PresetData.h"

namespace pon
{
std::unique_ptr<juce::XmlElement> presetDataToXml (const PresetData& d)
{
    auto xml = std::make_unique<juce::XmlElement> ("Internal");

   #define PON_WRITE(name, def) xml->setAttribute (#name, (double) d.name);
    PON_GLOBAL_FIELDS (PON_WRITE)
   #undef PON_WRITE

    for (int b = 0; b < 3; ++b)
    {
        auto* e = xml->createNewChildElement ("Band");
        e->setAttribute ("index", b);
        const auto& bp = d.band[b];
       #define PON_WRITE(name, def) e->setAttribute (#name, (double) bp.name);
        PON_BAND_FIELDS (PON_WRITE)
       #undef PON_WRITE
    }
    return xml;
}

PresetData presetDataFromXml (const juce::XmlElement& xml)
{
    PresetData d;
   #define PON_READ(name, def) d.name = (float) xml.getDoubleAttribute (#name, (double) d.name);
    PON_GLOBAL_FIELDS (PON_READ)
   #undef PON_READ

    for (auto* e : xml.getChildWithTagNameIterator ("Band"))
    {
        const int b = e->getIntAttribute ("index", -1);
        if (b < 0 || b > 2) continue;
        auto& bp = d.band[b];
       #define PON_READ(name, def) bp.name = (float) e->getDoubleAttribute (#name, (double) bp.name);
        PON_BAND_FIELDS (PON_READ)
       #undef PON_READ
    }

    // 不正値の保護
    auto fix = [] (float& v, float lo, float hi, float def)
    {
        if (! std::isfinite (v)) v = def;
        v = juce::jlimit (lo, hi, v);
    };
    fix (d.dlyFb, 0.0f, 0.95f, 0.35f);
    for (auto& bp : d.band)
    {
        fix (bp.grnSize, 5.0f, 400.0f, 60.0f);
        fix (bp.grnPitch, -24.0f, 24.0f, 0.0f);
        fix (bp.grnVoices, 1.0f, 4.0f, 1.0f);
        fix (bp.panHaas, 0.0f, 30.0f, 0.0f);
        fix (bp.wahQ, 0.3f, 20.0f, 2.5f);
        fix (bp.mul, 0.0f, 16.0f, 1.0f);
        fix (bp.dlyMul, 0.0f, 16.0f, 1.0f);
    }
    return d;
}

} // namespace pon
