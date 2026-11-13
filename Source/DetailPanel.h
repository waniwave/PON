#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetData.h"
#include <array>
#include <vector>

class PonProcessor;

// 詳細パラメーター（プリセットの内部値）を編集するパネル。
// タブごとに「1 行 = 1 項目、列 = LOW / MID / HIGH」。全体の項目は 3 列ぶち抜き。
// 編集はすぐ音に反映され（クロスフェードなし）、プロジェクトとユーザープリセットに保存される。
class DetailPanel : public juce::Component
{
public:
    explicit DetailPanel (PonProcessor& p);
    ~DetailPanel() override;

    void refresh();          // プロセッサーの現在値を表示に反映
    void paint (juce::Graphics& g) override;
    void resized() override;

    struct Choice { float value; const char* text; };
    enum class Kind { Float, Choice, Toggle };
    enum class Unit { None, Percent, Hz, Ms, Semitone, Beat, Degree, Ratio, Times };

    struct Desc
    {
        const char* label;
        float pon::PresetData::* g;    // 全体の項目（バンドの項目なら nullptr）
        float pon::BandParams::* b;    // バンドの項目（全体の項目なら nullptr）
        Kind kind;
        float min, max, step;
        float mid;                     // > 0 ならこの値がスライダーの真ん中（対数風）
        Unit unit;
        const Choice* choices;
        int numChoices;
        const char* tip;
    };

    struct Tab
    {
        const char* name;
        std::vector<Desc> rows;
    };

private:
    struct Cell
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        juce::Component* comp() const { return slider != nullptr ? (juce::Component*) slider.get() : (juce::Component*) combo.get(); }
    };
    struct Row
    {
        const Desc* desc = nullptr;
        std::unique_ptr<juce::Label> label;
        std::vector<Cell> cells;   // 全体 = 1 個、バンド = 3 個
    };

    void buildTab (int index);
    Cell makeCell (const Desc& d, int band);
    float readValue (const Desc& d, int band, const pon::PresetData& data) const;
    void writeValue (const Desc& d, int band, float v);
    void setCellValue (Cell& c, const Desc& d, float v, float base);

    PonProcessor& proc;
    std::vector<Tab> tabs;
    std::vector<std::unique_ptr<juce::TextButton>> tabButtons;
    juce::TextButton revertButton { "REVERT" };
    std::vector<Row> rows;
    int currentTab = 1;
    bool updating = false;
};
