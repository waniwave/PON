#pragma once

#include <juce_core/juce_core.h>

namespace pon
{
// 内部パラメーター（ユーザーには見せない）。すべて float の POD なので、音声スレッドで安全にコピーできる。
// X マクロで並べてあるので、項目を足すと XML の保存/読込にも自動で反映される。
//
// 単位:
//   mul / wahMul / dlyMul … R（1拍あたりの回数）に掛ける倍率。mul = 0 は「—」（そのバンドに効かせない）
//   *Amt                  … そのバンドへの配分 0..1（プリセットの WAH/PAN/DLY/GRN に掛かる）
//   grnSize ms, grnOffset/grnJitter 拍, grnPitch/grnSpread 半音, panHaas ms, *HP/*LP Hz
#define PON_BAND_FIELDS(X) \
    X(on, 1.0f) X(mul, 1.0f) \
    X(wahAmt, 1.0f) X(wahLo, 350.0f) X(wahHi, 2800.0f) X(wahQ, 2.5f) X(wahLp, 0.0f) \
    X(wahShape, 0.0f) X(wahInv, 0.0f) X(wahPhase, 0.0f) X(wahMul, 1.0f) \
    X(grnAmt, 1.0f) X(grnSize, 60.0f) X(grnSub, 2.0f) X(grnVoices, 1.0f) X(grnPitch, 0.0f) \
    X(grnSpread, 0.0f) X(grnOct, 0.0f) X(grnOffset, 0.25f) X(grnJitter, 0.25f) X(grnRev, 0.0f) \
    X(grnTail, 0.0f) X(grnWidth, 0.5f) \
    X(panAmt, 1.0f) X(panL, 1.0f) X(panR, 1.0f) X(panPhase, 0.0f) X(panSoft, 0.3f) X(panHaas, 0.0f) \
    X(dlyAmt, 1.0f) X(dlyMul, 1.0f)

//   wah/pan/dly/grn       … RADIUS 最大のときの各モジュールの効き 0..1（表の WAH/PAN/DLY/GRN）
//   curve                 … RADIUS カーブの急さ
//   dlyFbType             … 0:なし 1:LP 2:HP 3:BP（フィードバック経路）
//   rotate / rotRate      … 擬似スペクトル回転（1 拍あたりの回転数）
//   grnDrift / grnDriftBars … グレインのピッチを何小節かけて何半音下げるか
//   grnWob / grnWobRate   … グレインのピッチのゆっくりした揺れ（半音 / 1拍あたり）
//   sp*                   … SPACE（リバーブの大きさ・ダンプ・プリディレイ[拍]・Haas[ms]）
//   rec*                  … SPACE / CHAOS / PUNCH の推奨値
#define PON_GLOBAL_FIELDS(X) \
    X(wah, 0.0f) X(pan, 0.0f) X(dly, 0.0f) X(grn, 0.0f) X(curve, 3.0f) \
    X(dlyFb, 0.35f) X(dlyPing, 0.0f) X(dlyFbType, 1.0f) X(dlyFbFreq, 6000.0f) X(dlyFbQ, 0.7f) \
    X(dlyWah, 0.0f) X(dlyRev, 0.0f) X(dlyOutHP, 20.0f) X(dlyOutLP, 20000.0f) \
    X(wetHP, 20.0f) X(wetLP, 20000.0f) \
    X(rotate, 0.0f) X(rotRate, 0.0625f) \
    X(grnDrift, 0.0f) X(grnDriftBars, 8.0f) X(grnWob, 0.0f) X(grnWobRate, 0.25f) \
    X(spSize, 0.4f) X(spDamp, 0.5f) X(spPre, 0.25f) X(spHaas, 12.0f) \
    X(recSpace, 0.25f) X(recChaos, 0.1f) X(recPunch, 0.6f)

struct BandParams
{
   #define PON_DECLARE(name, def) float name = def;
    PON_BAND_FIELDS (PON_DECLARE)
   #undef PON_DECLARE
};

struct PresetData
{
   #define PON_DECLARE(name, def) float name = def;
    PON_GLOBAL_FIELDS (PON_DECLARE)
   #undef PON_DECLARE
    BandParams band[3];
};

// <Internal> 要素として保存/読込（存在しない項目はデフォルト値のまま）
std::unique_ptr<juce::XmlElement> presetDataToXml (const PresetData& d);
PresetData presetDataFromXml (const juce::XmlElement& xml);

} // namespace pon
