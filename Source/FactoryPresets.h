#pragma once

#include "PresetData.h"
#include <vector>

namespace pon
{
struct FactoryPreset
{
    juce::String id;          // 英語の内部 ID（例: drift_lukewarm_fan）
    juce::String category;    // DRIFT / ORBIT / ECHO / DUST / CHAOS
    juce::String name;        // 表示名（日本語）
    juce::String description; // 説明（日本語）
    PresetData data;
};

// 最初の呼び出しはメッセージスレッドで（Processor のコンストラクタ）。以降は読み取り専用。
const std::vector<FactoryPreset>& getFactoryPresets();
const juce::StringArray& getCategoryNames();
int getDefaultPresetIndex();   // 七つ星

} // namespace pon
