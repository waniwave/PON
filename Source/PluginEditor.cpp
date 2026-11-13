#include "PluginEditor.h"

using namespace pon::ui;

juce::AudioProcessorEditor* PonProcessor::createEditor()
{
    return new PonEditor (*this);
}

PonEditor::PonEditor (PonProcessor& p)
    : AudioProcessorEditor (&p), proc (p), pad (p), detail (p)
{
    setLookAndFeel (&lnf);

    // プリセット: カテゴリ > 名前 の階層
    {
        const auto& presets = pon::getFactoryPresets();
        auto* root = presetBox.getRootMenu();
        for (const auto& cat : pon::getCategoryNames())
        {
            juce::PopupMenu sub;
            for (int i = 0; i < (int) presets.size(); ++i)
                if (presets[(size_t) i].category == cat)
                    sub.addItem (i + 1, presets[(size_t) i].name);
            root->addSubMenu (cat, sub);
        }
        presetBox.setJustificationType (juce::Justification::centred);
        presetBox.setTextWhenNothingSelected ("-");
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();
            if (id > 0) proc.selectFactoryPreset (id - 1);
            refreshPresetDisplay();
        };
        addAndMakeVisible (presetBox);
    }

    prevButton.onClick = [this] { proc.stepPreset (-1); refreshPresetDisplay(); };
    nextButton.onClick = [this] { proc.stepPreset (+1); refreshPresetDisplay(); };
    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPreset(); };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &loadButton })
        addAndMakeVisible (*b);

    snapAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pon::ids::snap, snapButton);
    anchorAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pon::ids::anchor, anchorButton);
    snapButton.setTooltip ("Snap the angle to the nearest spoke");
    anchorButton.setTooltip ("Pull the pan to the centre on every downbeat");
    addAndMakeVisible (snapButton);
    addAndMakeVisible (anchorButton);

    descLabel.setFont (uiFont (13.0f));
    descLabel.setColour (juce::Label::textColourId, col::dim);
    descLabel.setJustificationType (juce::Justification::centredLeft);
    descLabel.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (descLabel);

    addAndMakeVisible (pad);

    // 詳細パラメーター（閉じておいて、DETAIL で右側に開く）
    detailButton.setClickingTogglesState (true);
    detailButton.setTooltip ("Show / hide detailed parameters");
    detailButton.onClick = [this] { setDetailOpen (detailButton.getToggleState()); };
    addAndMakeVisible (detailButton);
    addChildComponent (detail);

    const juce::String ids[5]   { pon::ids::mix, pon::ids::space, pon::ids::chaos, pon::ids::punch, pon::ids::output };
    const juce::String names[5] { "MIX", "SPACE", "CHAOS", "PUNCH", "OUTPUT" };
    for (int i = 0; i < 5; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setPopupDisplayEnabled (true, true, this, 1500);   // ドラッグ中とホバー時に値を表示
        k.slider.setScrollWheelEnabled (true);
        k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.slider);
        if (auto* prm = proc.apvts.getParameter (ids[i]))
            k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        addAndMakeVisible (k.slider);

        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setFont (uiFont (12.0f, true));
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, col::dim);
        addAndMakeVisible (k.label);
    }

    setDetailOpen (proc.isDetailOpen());
    refreshPresetDisplay();
    startTimerHz (30);
}

void PonEditor::setDetailOpen (bool open)
{
    proc.setDetailOpen (open);
    detailButton.setToggleState (open, juce::dontSendNotification);
    detailButton.setButtonText (juce::String ("DETAIL ") + juce::String::fromUTF8 (open ? "◀" : "▶"));
    detail.setVisible (open);
    if (open) detail.refresh();
    setSize (open ? kMainW + kDetailW : kMainW, kH);
}

PonEditor::~PonEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PonEditor::refreshPresetDisplay()
{
    lastUiVersion = proc.getUiVersion();
    lastPresetIndex = proc.getPresetParamIndex();

    // 詳細パネルで編集中は名前に「*」。同じプリセットを選び直すと編集前に戻る
    if (proc.isUserPresetActive() || proc.isModified())
        presetBox.setText (proc.getCurrentPresetName() + (proc.isModified() ? " *" : ""), juce::dontSendNotification);
    else
        presetBox.setSelectedId (lastPresetIndex + 1, juce::dontSendNotification);

    const auto& f = pon::getFactoryPresets()[(size_t) lastPresetIndex];
    descLabel.setText (proc.isUserPresetActive() ? juce::String ("User preset (" + f.category + ")")
                                                 : f.category + "  |  " + proc.getCurrentPresetDescription(),
                       juce::dontSendNotification);
}

void PonEditor::timerCallback()
{
    if (proc.getUiVersion() != lastUiVersion || proc.getPresetParamIndex() != lastPresetIndex)
    {
        refreshPresetDisplay();
        if (detail.isVisible() && proc.getDataVersion() == lastDataVersion)
            detail.refresh();   // REVERT ボタンの有効/無効など（値はそのまま）
    }
    if (proc.getDataVersion() != lastDataVersion)
    {
        lastDataVersion = proc.getDataVersion();   // プリセットが読み込まれた → 詳細パネルの値を入れ替える
        if (detail.isVisible()) detail.refresh();
    }

    const float peak = proc.getEngine().outPeak.exchange (0.0f);
    meterLevel = juce::jmax (peak, meterLevel * 0.86f);
    repaint (meterArea);
}

void PonEditor::savePreset()
{
    const auto folder = PonProcessor::getUserPresetFolder();
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save PON preset", folder.getChildFile (proc.getCurrentPresetName() + ".ponpreset"), "*.ponpreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == juce::File()) return;
                              if (! file.hasFileExtension ("ponpreset"))
                                  file = file.withFileExtension ("ponpreset");
                              proc.saveUserPreset (file);
                              refreshPresetDisplay();
                          });
}

void PonEditor::loadPreset()
{
    const auto folder = PonProcessor::getUserPresetFolder();
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Load PON preset", folder, "*.ponpreset");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile())
                                  proc.loadUserPreset (file);
                              refreshPresetDisplay();
                          });
}

void PonEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // 上部バー
    g.setColour (col::panel);
    g.fillRect (0, 0, getWidth(), 84);
    g.setColour (col::outline);
    g.drawHorizontalLine (84, 0.0f, (float) getWidth());

    // ロゴ
    g.setFont (uiFont (30.0f, true));
    g.setColour (col::accent);
    g.drawText ("PON", 16, 8, 80, 36, juce::Justification::centredLeft);
    g.setColour (col::accent2);
    g.fillEllipse (84.0f, 14.0f, 7.0f, 7.0f);

    // 出力メーター
    {
        auto r = meterArea.toFloat();
        g.setColour (col::dim);
        g.setFont (uiFont (10.0f, true));
        g.drawText ("OUT", r.removeFromLeft (28.0f), juce::Justification::centredLeft);
        g.setColour (col::bg);
        g.fillRoundedRectangle (r, 3.0f);
        const float db = juce::Decibels::gainToDecibels (meterLevel, -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = r.reduced (1.5f);
        fill.setWidth (fill.getWidth() * frac);
        g.setColour (db > -1.0f ? col::accent : (db > -9.0f ? col::mid : col::high));
        g.fillRoundedRectangle (fill, 2.0f);
    }

    // ノブ列の区切り
    g.setColour (col::outline);
    g.drawHorizontalLine (652, 24.0f, (float) kMainW - 24.0f);
}

void PonEditor::resized()
{
    prevButton.setBounds (104, 12, 28, 30);
    presetBox.setBounds (136, 12, 264, 30);
    nextButton.setBounds (404, 12, 28, 30);
    saveButton.setBounds (444, 12, 86, 30);
    loadButton.setBounds (538, 12, 86, 30);

    snapButton.setBounds (16, 50, 70, 26);
    anchorButton.setBounds (92, 50, 84, 26);
    descLabel.setBounds (184, 50, 222, 26);
    detailButton.setBounds (410, 50, 76, 26);
    detail.setBounds (kMainW, 0, kDetailW, kH);
    meterArea = { 492, 56, 132, 14 };

    pad.setBounds (40, 88, 560, 560);

    const int w = kMainW / 5;
    for (int i = 0; i < 5; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.slider.setBounds (i * w + (w - 80) / 2, 660, 80, 80);
        k.label.setBounds (i * w, 742, w, 20);
    }
}
