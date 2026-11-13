#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "FactoryPresets.h"
#include "Params.h"

class PonProcessor : public juce::AudioProcessor,
                     private juce::Timer
{
public:
    PonProcessor();
    ~PonProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PON"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- メッセージスレッド用
    juce::AudioProcessorValueTreeState apvts;
    pon::Engine& getEngine() noexcept { return engine; }

    void selectFactoryPreset (int index);
    void stepPreset (int delta);
    bool saveUserPreset (const juce::File& file);
    bool loadUserPreset (const juce::File& file);
    static juce::File getUserPresetFolder();

    juce::String getCurrentPresetName() const { return currentName; }
    juce::String getCurrentPresetDescription() const { return currentDescription; }
    bool isUserPresetActive() const { return currentIsUser; }
    int getPresetParamIndex() const;
    int getUiVersion() const noexcept { return uiVersion.load(); }

    // ---- 詳細パラメーター（メッセージスレッド用）
    const pon::PresetData& getCurrentData() const { return currentData; }
    const pon::PresetData& getBaseData() const { return baseData; }   // 読み込んだ時点の値（ダブルクリック/REVERT の戻り先）
    void editCurrentData (const std::function<void (pon::PresetData&)>& edit);
    void revertEdits();
    bool isModified() const { return currentModified; }
    int getDataVersion() const noexcept { return dataVersion.load(); } // プリセットが読み込まれるたびに増える
    bool isDetailOpen() const { return detailOpen; }
    void setDetailOpen (bool b) { detailOpen = b; }

    // PRESET パラメーターの変化を拾って、推奨値（SPACE/CHAOS/PUNCH）と表示を更新（タイマーから呼ばれる。テスト用に公開）
    void syncPresetFromParam();

private:
    void timerCallback() override { syncPresetFromParam(); }
    void pushPending (const pon::PresetData& d, int index, bool immediate, bool live = false);
    void setParamValue (const juce::String& id, float plainValue);

    pon::Engine engine;
    juce::AudioParameterChoice* presetParam = nullptr;
    std::atomic<float>* pAngle = nullptr; std::atomic<float>* pRadius = nullptr; std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pSpace = nullptr; std::atomic<float>* pChaos = nullptr; std::atomic<float>* pPunch = nullptr;
    std::atomic<float>* pOutput = nullptr; std::atomic<float>* pSnap = nullptr; std::atomic<float>* pAnchor = nullptr;

    // 音声スレッドへの受け渡し枠（メッセージスレッドが書き、音声スレッドは try-lock で取り込む）
    juce::SpinLock pendingLock;
    pon::PresetData pendingData;
    int pendingIndex = 0;
    bool pendingImmediate = false;
    bool pendingLive = false;
    int pendingVersion = 0;

    // 音声スレッド側
    int audioPresetIndex = -1;
    std::atomic<int> consumedVersion { 0 };
    std::atomic<double> lastBpm { 120.0 };

    // メッセージスレッド側の写し
    int mirrorIndex = -1;
    pon::PresetData currentData, baseData;
    juce::String currentName, currentDescription;
    bool currentIsUser = false;
    bool currentModified = false;
    bool detailOpen = false;
    std::atomic<int> uiVersion { 0 };
    std::atomic<int> dataVersion { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PonProcessor)
};
