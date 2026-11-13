#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace pon;

PonProcessor::PonProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PON", createParameterLayout())
{
    presetParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ids::preset));
    pAngle  = apvts.getRawParameterValue (ids::angle);
    pRadius = apvts.getRawParameterValue (ids::radius);
    pMix    = apvts.getRawParameterValue (ids::mix);
    pSpace  = apvts.getRawParameterValue (ids::space);
    pChaos  = apvts.getRawParameterValue (ids::chaos);
    pPunch  = apvts.getRawParameterValue (ids::punch);
    pOutput = apvts.getRawParameterValue (ids::output);
    pSnap   = apvts.getRawParameterValue (ids::snap);
    pAnchor = apvts.getRawParameterValue (ids::anchor);

    // 起動時: 七つ星（SPACE/CHAOS/PUNCH はパラメーターのデフォルト値のまま）
    const int idx = getPresetParamIndex();
    const auto& f = getFactoryPresets()[(size_t) idx];
    mirrorIndex = idx;
    currentData = f.data;
    baseData = f.data;
    currentName = f.name;
    currentDescription = f.description;
    // 音声スレッドは最初のブロックで PRESET パラメーターを見て即時に読み込む（audioPresetIndex = -1）

    startTimerHz (20);
}

PonProcessor::~PonProcessor()
{
    stopTimer();
}

int PonProcessor::getPresetParamIndex() const
{
    const int n = (int) getFactoryPresets().size();
    return presetParam != nullptr ? juce::jlimit (0, n - 1, presetParam->getIndex()) : getDefaultPresetIndex();
}

bool PonProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void PonProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
    // Engine は prepare しても現在のプリセット内容を保持する（audioPresetIndex もそのまま）
}

void PonProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    const int numIn = getTotalNumInputChannels();   // 1（モノ）または 2。モノのときは Engine が L を R に複製する

    // PRESET パラメーター（オートメーション可能）
    const auto& presets = getFactoryPresets();
    const int idx = getPresetParamIndex();
    if (idx != audioPresetIndex)
    {
        const bool immediate = audioPresetIndex < 0;
        audioPresetIndex = idx;
        engine.setPreset (presets[(size_t) idx].data, immediate);
    }

    // ユーザープリセット / 状態復元（待たない try-lock）
    {
        const juce::SpinLock::ScopedTryLockType tl (pendingLock);
        if (tl.isLocked() && pendingVersion != consumedVersion)
        {
            consumedVersion = pendingVersion;
            audioPresetIndex = pendingIndex;
            if (pendingLive && ! pendingImmediate)
                engine.updatePresetLive (pendingData);
            else
                engine.setPreset (pendingData, pendingImmediate);
            pendingImmediate = false;
            pendingLive = false;
        }
    }

    EngineInput in;
    in.angle    = pAngle->load();
    in.radius   = pRadius->load();
    in.mix      = pMix->load();
    in.space    = pSpace->load();
    in.chaos    = pChaos->load();
    in.punch    = pPunch->load();
    in.outputDb = pOutput->load();
    in.snap     = pSnap->load() > 0.5f;
    in.anchor   = pAnchor->load() > 0.5f;

    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            in.playing = pos->getIsPlaying();
            if (const auto ppq = pos->getPpqPosition())                { in.hasPpq = true; in.ppq = *ppq; }
            if (const auto bpm = pos->getBpm())                        { in.hasBpm = true; in.bpm = *bpm; }
            if (const auto bar = pos->getPpqPositionOfLastBarStart())  { in.hasBarStart = true; in.barStart = *bar; }
            if (const auto ts = pos->getTimeSignature())               { in.timeSigNum = ts->numerator; in.timeSigDen = ts->denominator; }
        }
    }
    if (in.hasBpm) lastBpm.store (in.bpm, std::memory_order_relaxed);

    engine.process (buffer, juce::jmin (2, numIn), in);
}

double PonProcessor::getTailLengthSeconds() const
{
    // ディレイ（最長 4 拍 × -60dB までの回数）＋ リバーブ（最大約 3.5 秒）
    const double bpm = juce::jlimit (40.0, 300.0, lastBpm.load());
    const auto& d = currentData;
    double delayTail = 0.0;
    if (d.dly > 0.0f)
    {
        double maxBeats = 0.0;
        for (const auto& b : d.band)
            if (b.on > 0.5f && b.dlyMul > 0.0f)
                maxBeats = juce::jmax (maxBeats, juce::jmin (SyncDelay::kMaxBeats, 2.0 / (double) b.dlyMul)); // R=1/2 のとき
        const double fb = juce::jlimit (0.0, 0.95, (double) d.dlyFb);
        const double repeats = fb > 0.001 ? juce::jmin (150.0, std::log (0.001) / std::log (fb)) : 1.0;
        delayTail = maxBeats * 60.0 / bpm * (repeats + 1.0);
    }
    return juce::jmin (60.0, delayTail + 3.5);
}

// ------------------------------------------------------------------------------------------------
void PonProcessor::pushPending (const PresetData& d, int index, bool immediate, bool live)
{
    const juce::SpinLock::ScopedLockType sl (pendingLock);
    // まだ取り込まれていない「読み込み」（クロスフェード/即時）を、編集で上書きして消さない
    const bool unconsumed = consumedVersion.load() != pendingVersion;
    if (unconsumed)
    {
        live = live && pendingLive;
        immediate = immediate || pendingImmediate;
    }
    pendingData = d;
    pendingIndex = index;
    pendingImmediate = immediate;
    pendingLive = live;
    ++pendingVersion;
}

void PonProcessor::editCurrentData (const std::function<void (PresetData&)>& edit)
{
    edit (currentData);
    currentModified = true;
    mirrorIndex = getPresetParamIndex();
    pushPending (currentData, mirrorIndex, false, true);
    ++uiVersion;
}

void PonProcessor::revertEdits()
{
    currentData = baseData;
    currentModified = false;
    mirrorIndex = getPresetParamIndex();
    pushPending (currentData, mirrorIndex, false, true);
    ++uiVersion;
    ++dataVersion;
}

void PonProcessor::setParamValue (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void PonProcessor::syncPresetFromParam()
{
    const int idx = getPresetParamIndex();
    if (idx == mirrorIndex) return;

    const auto& f = getFactoryPresets()[(size_t) idx];
    mirrorIndex = idx;
    currentData = f.data;
    baseData = f.data;
    currentName = f.name;
    currentDescription = f.description;
    currentIsUser = false;
    currentModified = false;
    ++dataVersion;

    // 推奨値を読み込む（ANGLE/RADIUS/MIX/OUTPUT/SNAP/ANCHOR は変えない）
    setParamValue (ids::space, f.data.recSpace);
    setParamValue (ids::chaos, f.data.recChaos);
    setParamValue (ids::punch, f.data.recPunch);
    ++uiVersion;
}

void PonProcessor::selectFactoryPreset (int index)
{
    const int n = (int) getFactoryPresets().size();
    index = ((index % n) + n) % n;

    if (index == getPresetParamIndex())
    {
        // 同じ番号を選び直した（ユーザープリセットから戻る等）→ 強制的に読み直す
        mirrorIndex = -1;
        pushPending (getFactoryPresets()[(size_t) index].data, index, false);
    }
    else if (presetParam != nullptr)
    {
        presetParam->beginChangeGesture();
        presetParam->setValueNotifyingHost (presetParam->convertTo0to1 ((float) index));
        presetParam->endChangeGesture();
    }
    syncPresetFromParam();
}

void PonProcessor::stepPreset (int delta)
{
    selectFactoryPreset (getPresetParamIndex() + delta);
}

juce::File PonProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("PON").getChildFile ("Presets");
}

bool PonProcessor::saveUserPreset (const juce::File& file)
{
    juce::XmlElement xml ("PONPreset");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("name", file.getFileNameWithoutExtension());
    xml.setAttribute ("basedOn", getFactoryPresets()[(size_t) getPresetParamIndex()].id);
    xml.setAttribute ("space", (double) pSpace->load());
    xml.setAttribute ("chaos", (double) pChaos->load());
    xml.setAttribute ("punch", (double) pPunch->load());
    xml.addChildElement (presetDataToXml (currentData).release());

    file.getParentDirectory().createDirectory();
    if (! xml.writeTo (file)) return false;

    currentName = file.getFileNameWithoutExtension();
    currentDescription = "User preset";
    currentIsUser = true;
    currentModified = false;
    baseData = currentData;
    ++uiVersion;
    return true;
}

bool PonProcessor::loadUserPreset (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("PONPreset")) return false;

    PresetData d = currentData;
    if (const auto* internal = xml->getChildByName ("Internal"))
        d = presetDataFromXml (*internal);

    setParamValue (ids::space, (float) xml->getDoubleAttribute ("space", d.recSpace));
    setParamValue (ids::chaos, (float) xml->getDoubleAttribute ("chaos", d.recChaos));
    setParamValue (ids::punch, (float) xml->getDoubleAttribute ("punch", d.recPunch));

    currentData = d;
    baseData = d;
    currentName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    currentDescription = "User preset";
    currentIsUser = true;
    currentModified = false;
    mirrorIndex = getPresetParamIndex();
    pushPending (d, mirrorIndex, false);
    ++uiVersion;
    ++dataVersion;
    return true;
}

// ------------------------------------------------------------------------------------------------
void PonProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("PONState");
    root.setAttribute ("version", 1);

    // タイマーより先に呼ばれた場合に備え、PRESET パラメーターの値と内部データを一致させる
    const int idx = getPresetParamIndex();
    const bool mirrorValid = (idx == mirrorIndex);
    const auto& f = getFactoryPresets()[(size_t) idx];
    const PresetData& d = mirrorValid ? currentData : f.data;

    root.setAttribute ("presetName", mirrorValid ? currentName : f.name);
    root.setAttribute ("presetDescription", mirrorValid ? currentDescription : f.description);
    root.setAttribute ("isUser", (mirrorValid && currentIsUser) ? 1 : 0);
    root.setAttribute ("modified", (mirrorValid && currentModified) ? 1 : 0);
    root.setAttribute ("detailOpen", detailOpen ? 1 : 0);

    if (auto params = apvts.copyState().createXml())
        root.addChildElement (params.release());
    root.addChildElement (presetDataToXml (d).release());

    copyXmlToBinary (root, destData);
}

void PonProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto root = getXmlFromBinary (data, sizeInBytes);
    if (root == nullptr || ! root->hasTagName ("PONState")) return;

    if (const auto* params = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    const int idx = getPresetParamIndex();
    const auto& f = getFactoryPresets()[(size_t) idx];

    PresetData d = f.data;
    if (const auto* internal = root->getChildByName ("Internal"))
        d = presetDataFromXml (*internal);

    mirrorIndex = idx;   // 推奨値で上書きしない（保存された SPACE/CHAOS/PUNCH を使う）
    currentData = d;
    currentName = root->getStringAttribute ("presetName", f.name);
    currentDescription = root->getStringAttribute ("presetDescription", f.description);
    currentIsUser = root->getIntAttribute ("isUser", 0) != 0;
    currentModified = root->getIntAttribute ("modified", 0) != 0;
    baseData = currentIsUser ? d : f.data;   // REVERT の戻り先
    detailOpen = root->getIntAttribute ("detailOpen", 0) != 0;

    pushPending (d, idx, true);
    ++uiVersion;
    ++dataVersion;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PonProcessor();
}
