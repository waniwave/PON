// ファクトリープリセット（5 カテゴリ × 9 = 45 個）
//
// 追加するとき: kDefs に 1 行足すだけ。
//   { id, カテゴリ, 名前, 説明, WAH, PAN, DLY, GRN, 倍率L, 倍率M, 倍率H, SPACE, CHAOS, PUNCH, 細かい調整 }
//   倍率 0 = 「—」（そのバンドには効かせない）。細かい調整は PresetData の各項目を直接書き換えるラムダ。
#include "FactoryPresets.h"

namespace pon
{
namespace
{
constexpr float OFF = 0.0f;

using Tweak = void (*) (PresetData&);

struct Def
{
    const char* id;
    const char* cat;
    const char* name;
    const char* desc;
    float wah, pan, dly, grn;
    float mL, mM, mH;
    float S, C, P;
    Tweak tweak;
};

template <typename F>
void forBands (PresetData& p, F f)
{
    for (auto& b : p.band) f (b);
}

inline void wahRange (BandParams& b, float lo, float hi) { b.wahLo = lo; b.wahHi = hi; }

// グレインの基本設定
inline void grain (BandParams& b, float sizeMs, float sub, float voices, float offsetBeats, float jitterBeats, float width)
{
    b.grnSize = sizeMs; b.grnSub = sub; b.grnVoices = voices;
    b.grnOffset = offsetBeats; b.grnJitter = jitterBeats; b.grnWidth = width;
}

inline void delayFb (PresetData& p, float fb, int type, float freq, float q = 0.7f)
{
    p.dlyFb = fb; p.dlyFbType = (float) type; p.dlyFbFreq = freq; p.dlyFbQ = q;
}

const Def kDefs[] =
{
    // ================================================================ DRIFT（ワウ中心・軽め）
    { "drift_lukewarm_fan", "DRIFT", "ぬるい扇風機", "全帯域が同じ周期でゆっくり首振り。Q低めの入門用ワウ。",
      60, 15, 0, 0,  0.25f, 0.25f, 0.25f,  20, 10, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.wahQ = 1.2f; b.wahLp = 0.35f; b.panSoft = 1.0f; });
      } },

    { "drift_aquarium", "DRIFT", "水槽ワウ", "中域だけ深いワウ。泡っぽく丸い音。",
      70, 0, 10, 0,  OFF, 0.5f, OFF,  25, 15, 60,
      [] (PresetData& p) {
          auto& m = p.band[1];
          wahRange (m, 280.0f, 2000.0f); m.wahQ = 4.0f; m.wahLp = 0.15f;
          delayFb (p, 0.3f, 1, 2500.0f);
          p.spSize = 0.5f;
      } },

    { "drift_hangover", "DRIFT", "二日酔い", "帯域ごとに周期が少しずつずれ、数小節かけてうねる。",
      60, 10, 0, 0,  0.25f, 1.0f / 3.0f, 0.375f,  20, 20, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.wahQ = 2.0f; b.wahLp = 0.3f; b.panSoft = 0.8f; });
      } },

    { "drift_reverse_wah", "DRIFT", "逆ワウ", "逆ワウモード。音が大きいほど閉じ、隙間で開く。",
      70, 0, 0, 0,  0.5f, 0.5f, 0.5f,  15, 10, 50,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.wahInv = 1.0f; b.wahQ = 3.0f; b.wahLp = 0.2f; });
          wahRange (p.band[0], 100.0f, 500.0f);
          wahRange (p.band[1], 300.0f, 3000.0f);
          wahRange (p.band[2], 2500.0f, 10000.0f);
      } },

    { "drift_radio", "DRIFT", "ラジオ越し", "ウェットを300Hz〜3kHzに絞ってワウ。ローファイ。",
      50, 0, 15, 0,  0.5f, 0.5f, 0.5f,  15, 15, 55,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.wahQ = 2.5f; });
          p.wetHP = 300.0f; p.wetLP = 3000.0f;
          p.dlyOutHP = 300.0f; p.dlyOutLP = 3000.0f;
          delayFb (p, 0.3f, 1, 3000.0f);
      } },

    { "drift_talking_hats", "DRIFT", "ハットだけ喋る", "高域だけ速いワウ。ハットがしゃべるように動く。",
      70, 0, 0, 0,  OFF, OFF, 2.0f,  10, 15, 60,
      [] (PresetData& p) {
          auto& h = p.band[2];
          wahRange (h, 2500.0f, 11000.0f); h.wahQ = 4.5f; h.wahShape = 1.0f; h.wahLp = 0.0f;
      } },

    { "drift_afternoon_curtain", "DRIFT", "午後のカーテン", "薄いワウに薄いディレイ。BGM向け。",
      35, 10, 25, 0,  0.5f, 0.5f, 0.5f,  35, 10, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.wahQ = 1.5f; b.wahLp = 0.4f; b.panSoft = 1.0f; });
          delayFb (p, 0.35f, 1, 4000.0f);
          p.spSize = 0.6f; p.spDamp = 0.6f;
      } },

    { "drift_rubber_wall", "DRIFT", "ゴムの壁", "高レゾナンスで粘る共振ワウ。",
      80, 0, 0, 0,  0.5f, 1.0f, 1.0f,  15, 15, 65,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.wahQ = 8.0f; b.wahLp = 0.8f; });
          wahRange (p.band[0], 100.0f, 450.0f);
          wahRange (p.band[1], 300.0f, 2200.0f);
          wahRange (p.band[2], 2000.0f, 7000.0f);
      } },

    { "drift_three_blades", "DRIFT", "3枚刃", "低・中・高が4分・3連・8分の別周期で揺れる。",
      70, 10, 0, 0,  1.0f, 3.0f, 2.0f,  20, 15, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.wahQ = 3.0f; b.wahShape = 1.0f; b.wahLp = 0.1f; b.panSoft = 0.5f; });
      } },

    // ================================================================ ORBIT（パン中心・ポリリズム）
    { "orbit_pendulum", "ORBIT", "振り子", "左右1:1の素直な往復。基準の音。",
      0, 70, 0, 0,  1.0f, 1.0f, 1.0f,  20, 0, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.panSoft = 0.5f; });
      } },

    { "orbit_left3_right2", "ORBIT", "左3右2", "L:R＝3:2の偏ったパン。",
      0, 80, 0, 0,  1.0f, 1.0f, 1.0f,  20, 10, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.panL = 3.0f; b.panR = 2.0f; b.panSoft = 0.2f; });
      } },

    { "orbit_quintuplet_satellite", "ORBIT", "5連の衛星", "高域だけ5倍速で周回し、中域はゆったり回る。",
      10, 70, 0, 0,  OFF, 1.0f, 5.0f,  25, 10, 60,
      [] (PresetData& p) {
          p.band[1].panSoft = 0.8f;
          p.band[2].panSoft = 0.6f;
          forBands (p, [] (BandParams& b) { b.wahQ = 2.0f; b.wahLp = 0.3f; });
      } },

    { "orbit_seven_stars", "ORBIT", "七つ星", "低・中・高が3・5・7のポリリズムでパン。看板プリセット。",
      0, 85, 0, 0,  3.0f, 5.0f, 7.0f,  25, 10, 65,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.panSoft = 0.35f; });
      } },

    { "orbit_crossing", "ORBIT", "交差点", "中域だけ位相反転。帯域同士が逆方向にすれ違う。",
      0, 80, 0, 0,  1.0f, 1.0f, 1.0f,  20, 10, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.panSoft = 0.4f; });
          p.band[1].panPhase = 0.5f;
      } },

    { "orbit_staggering", "ORBIT", "千鳥足", "行き先がランダムに入れ替わり、よろける。",
      0, 75, 0, 0,  1.0f, 1.0f, 2.0f,  20, 60, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.panSoft = 0.25f; });
      } },

    { "orbit_balance_toy", "ORBIT", "ヤジロベエ", "中央付近で小刻みに揺れる。",
      0, 40, 0, 0,  2.0f, 2.0f, 2.0f,  15, 20, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.panSoft = 1.0f; });
          p.band[1].panPhase = 0.25f;
          p.band[2].panPhase = 0.5f;
      } },

    { "orbit_stereo_lost", "ORBIT", "ステレオ迷子", "拍に合わせたパンに、Haasの幅（最大25ms）の切り替えを足す。",
      0, 70, 0, 0,  1.0f, 1.0f, 1.0f,  50, 20, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.panHaas = 25.0f; b.panSoft = 0.2f; });
          p.band[1].panPhase = 0.25f;
          p.spHaas = 18.0f;
      } },

    { "orbit_sushi_go_round", "ORBIT", "回転寿司", "擬似スペクトル回転。3帯域が120°ずつずれて回り、音像が螺旋状に回る。",
      0, 80, 0, 0,  0.5f, 0.5f, 0.5f,  40, 10, 60,
      [] (PresetData& p) {
          p.rotate = 1.0f; p.rotRate = 1.0f / 16.0f;
          forBands (p, [] (BandParams& b) { b.panSoft = 1.0f; b.panHaas = 6.0f; });
      } },

    // ================================================================ ECHO（ディレイ中心）
    { "echo_parrot", "ECHO", "オウム返し", "1拍遅れで1回だけ返す（フィードバックなし）。",
      0, 10, 60, 0,  1.0f, 1.0f, 1.0f,  15, 0, 65,
      [] (PresetData& p) {
          delayFb (p, 0.0f, 0, 8000.0f);
          forBands (p, [] (BandParams& b) { b.panSoft = 0.5f; });
      } },

    { "echo_stairs", "ECHO", "階段エコー", "付点8分のディレイ。繰り返すたびに暗くなる。",
      0, 10, 70, 0,  4.0f / 3.0f, 4.0f / 3.0f, 4.0f / 3.0f,  25, 10, 65,
      [] (PresetData& p) {
          delayFb (p, 0.55f, 1, 2200.0f);
      } },

    { "echo_pingpong_lost", "ECHO", "ピンポン迷子", "帯域ごとに違う分割でピンポン。",
      0, 20, 65, 0,  1.0f, 1.5f, 2.0f,  30, 20, 60,
      [] (PresetData& p) {
          p.dlyPing = 1.0f;
          delayFb (p, 0.45f, 1, 6000.0f);
      } },

    { "echo_quintuplet_echo", "ECHO", "5連こだま", "5連のディレイ。",
      0, 10, 60, 0,  5.0f, 5.0f, 5.0f,  25, 10, 65,
      [] (PresetData& p) {
          delayFb (p, 0.45f, 1, 5000.0f);
      } },

    { "echo_tunnel", "ECHO", "トンネル", "フィードバック85%、LP 1.5kHzの暗いディレイ。",
      0, 0, 70, 0,  0.5f, 0.5f, 0.5f,  50, 10, 65,
      [] (PresetData& p) {
          delayFb (p, 0.85f, 1, 1500.0f, 0.6f);
          p.spSize = 0.7f; p.spDamp = 0.7f;
      } },

    { "echo_melting_mirror", "ECHO", "溶ける鏡", "ディレイのループ内でワウが動く。",
      30, 10, 60, 0,  1.0f, 1.0f, 1.0f,  30, 15, 60,
      [] (PresetData& p) {
          p.dlyWah = 0.8f;
          delayFb (p, 0.6f, 1, 7000.0f);
          forBands (p, [] (BandParams& b) { b.wahQ = 2.0f; b.wahLp = 0.2f; });
      } },

    { "echo_dub_room", "ECHO", "ダブ部屋", "ダブ風。フィードバック75%、HPで低域を逃がす。",
      0, 15, 70, 0,  4.0f / 3.0f, 4.0f / 3.0f, 4.0f / 3.0f,  70, 15, 65,
      [] (PresetData& p) {
          p.dlyPing = 1.0f;
          delayFb (p, 0.75f, 2, 400.0f);
          p.dlyOutLP = 6000.0f;
          p.spSize = 0.75f; p.spDamp = 0.55f;
      } },

    { "echo_phone", "ECHO", "電話の向こう", "ディレイ音を500Hz〜2.5kHzに絞る。",
      0, 0, 60, 0,  2.0f, 2.0f, 2.0f,  15, 10, 60,
      [] (PresetData& p) {
          p.dlyOutHP = 500.0f; p.dlyOutLP = 2500.0f;
          delayFb (p, 0.45f, 3, 1200.0f, 0.5f);
      } },

    { "echo_reverse_hint", "ECHO", "逆回転の気配", "ディレイ音だけが逆再生のグレインになる。",
      0, 10, 50, 40,  1.0f, 1.0f, 1.0f,  35, 20, 60,
      [] (PresetData& p) {
          p.dlyRev = 0.9f;
          delayFb (p, 0.4f, 1, 6000.0f);
          forBands (p, [] (BandParams& b) { grain (b, 120.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.6f); b.grnRev = 0.85f; });
      } },

    // ================================================================ DUST（グラニュラー中心）
    { "dust_sandstorm", "DUST", "砂嵐", "20ms前後の細かいグレインを高密度で。",
      0, 15, 0, 70,  OFF, 4.0f, 4.0f,  20, 40, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { grain (b, 20.0f, 2.0f, 3.0f, 0.1f, 0.5f, 0.8f); b.grnSpread = 2.0f; });
      } },

    { "dust_powder_hats", "DUST", "粉ハット", "高域だけグラニュラー。",
      0, 20, 0, 70,  OFF, OFF, 4.0f,  15, 30, 60,
      [] (PresetData& p) {
          auto& h = p.band[2];
          grain (h, 15.0f, 2.0f, 2.0f, 0.05f, 0.25f, 0.9f); h.grnSpread = 1.0f;
      } },

    { "dust_foggy_snare", "DUST", "霧のスネア", "余韻のみモード。アタックの後だけグレインになる。",
      0, 10, 10, 60,  OFF, 2.0f, 2.0f,  40, 20, 75,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { grain (b, 140.0f, 2.0f, 2.0f, 0.03f, 0.15f, 0.7f); b.grnTail = 1.0f; b.grnSpread = 0.3f; });
          delayFb (p, 0.3f, 1, 3000.0f);
          p.spSize = 0.6f;
      } },

    { "dust_glass_rain", "DUST", "ガラスの雨", "ピッチを+7〜+12半音に散らしたグレイン。",
      0, 25, 0, 60,  OFF, 2.0f, 4.0f,  30, 40, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { grain (b, 45.0f, 2.0f, 2.0f, 0.15f, 0.5f, 0.9f); b.grnPitch = 9.5f; b.grnSpread = 2.5f; });
      } },

    { "dust_konpeito", "DUST", "金平糖", "+12半音のグレインがきらめく。",
      0, 30, 0, 55,  OFF, 2.0f, 2.0f,  35, 30, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { grain (b, 35.0f, 2.0f, 2.0f, 0.1f, 0.3f, 1.0f); b.grnPitch = 12.0f; b.grnSpread = 0.15f; });
      } },

    { "dust_subway", "DUST", "地下鉄", "中域を-12半音のグレインで唸らせる。",
      0, 0, 0, 60,  OFF, 1.0f, OFF,  25, 20, 65,
      [] (PresetData& p) {
          auto& m = p.band[1];
          grain (m, 160.0f, 2.0f, 2.0f, 0.25f, 0.25f, 0.3f); m.grnPitch = -12.0f; m.grnSpread = 0.3f;
      } },

    { "dust_breathing_wall", "DUST", "呼吸する壁", "200〜400msの長いグレインでパッド状にする。",
      0, 10, 0, 70,  OFF, 0.5f, 0.5f,  60, 15, 70,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { grain (b, 300.0f, 8.0f, 2.0f, 0.5f, 1.0f, 0.7f); b.grnSpread = 0.2f; b.panSoft = 1.0f; });
          p.spSize = 0.8f;
      } },

    { "dust_fraying", "DUST", "ほつれ", "読み出し位置が大きくランダムにずれる。",
      0, 15, 0, 65,  OFF, 2.0f, 2.0f,  20, 60, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { grain (b, 80.0f, 1.0f, 1.0f, 0.25f, 4.0f, 0.6f); b.grnSpread = 0.5f; b.grnRev = 0.15f; });
      } },

    { "dust_old_vhs", "DUST", "古いVHS", "粗いグレインに、ゆっくりしたピッチの揺れ。",
      20, 0, 0, 50,  0.25f, 2.0f, 2.0f,  20, 25, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.wahQ = 1.0f; b.wahLp = 0.7f; grain (b, 90.0f, 1.0f, 1.0f, 0.12f, 0.1f, 0.4f); });
          p.band[0].grnAmt = 0.0f;
          p.grnWob = 0.45f; p.grnWobRate = 0.125f;
          p.wetLP = 6500.0f;
      } },

    // ================================================================ CHAOS（全部入り）
    { "chaos_spinning_room", "CHAOS", "回る部屋", "パンと空間が主役。部屋ごと回る。",
      0, 80, 30, 20,  0.5f, 0.5f, 1.0f,  80, 20, 60,
      [] (PresetData& p) {
          p.rotate = 1.0f; p.rotRate = 0.125f;
          forBands (p, [] (BandParams& b) { b.panSoft = 1.0f; grain (b, 150.0f, 2.0f, 1.0f, 0.25f, 0.5f, 0.8f); });
          p.dlyPing = 1.0f;
          delayFb (p, 0.45f, 1, 5000.0f);
          p.spSize = 0.85f; p.spDamp = 0.4f; p.spHaas = 20.0f;
      } },

    { "chaos_washing_machine", "CHAOS", "洗濯機", "ワウ・パン・ディレイが速く絡む。",
      70, 70, 50, 0,  2.0f, 3.0f, 4.0f,  30, 30, 65,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.wahQ = 3.0f; b.wahShape = 1.0f; b.panSoft = 0.3f; });
          p.dlyPing = 1.0f;
          delayFb (p, 0.5f, 1, 5000.0f);
      } },

    { "chaos_space_sick", "CHAOS", "宇宙酔い", "全帯域が3・5・7のポリリズムでパンし、ディレイも連動。",
      20, 80, 60, 0,  3.0f, 5.0f, 7.0f,  50, 30, 65,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.panSoft = 0.5f; b.panHaas = 8.0f; b.wahQ = 2.0f; b.wahLp = 0.3f; });
          p.dlyPing = 1.0f;
          delayFb (p, 0.45f, 1, 6000.0f);
      } },

    { "chaos_night_park", "CHAOS", "夜の遊園地", "オクターブ違いのグラニュラーとピンポンディレイ。",
      0, 40, 60, 60,  1.0f, 2.0f, 3.0f,  50, 30, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { grain (b, 70.0f, 1.0f, 2.0f, 0.25f, 0.5f, 0.9f); b.grnOct = 1.0f; b.grnSpread = 12.0f; });
          p.dlyPing = 1.0f;
          delayFb (p, 0.5f, 1, 5000.0f);
      } },

    { "chaos_kaleidoscope", "CHAOS", "万華鏡", "ピッチ違い（±12半音）のグレインが左右に散る。",
      0, 70, 0, 70,  OFF, 2.0f, 3.0f,  40, 50, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { grain (b, 60.0f, 2.0f, 3.0f, 0.2f, 0.5f, 1.0f); b.grnOct = 1.0f; b.grnSpread = 12.0f; b.panSoft = 0.4f; });
      } },

    { "chaos_game_center", "CHAOS", "ゲームセンター", "細かい刻みのグレインと速いワウでピコピコ。",
      60, 30, 0, 60,  OFF, 4.0f, 4.0f,  15, 30, 65,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) {
              b.wahQ = 5.0f; b.wahShape = 1.0f; b.wahLp = 0.0f;
              grain (b, 25.0f, 1.0f, 1.0f, 0.06f, 0.1f, 0.6f); b.grnPitch = 12.0f; b.grnOct = 1.0f; b.grnSpread = 12.0f;
          });
      } },

    { "chaos_crosstalk", "CHAOS", "混線", "低域はワウ、中域はディレイ、高域はグレインと、帯域ごとに別のエフェクトだけを鳴らす。",
      70, 40, 60, 60,  1.0f, 2.0f, 3.0f,  30, 40, 65,
      [] (PresetData& p) {
          auto& l = p.band[0]; auto& m = p.band[1]; auto& h = p.band[2];
          l.wahAmt = 1.0f; l.panAmt = 0.0f; l.dlyAmt = 0.0f; l.grnAmt = 0.0f; l.wahQ = 4.0f; l.wahLp = 0.5f;
          m.wahAmt = 0.0f; m.panAmt = 1.0f; m.dlyAmt = 1.0f; m.grnAmt = 0.0f;
          h.wahAmt = 0.0f; h.panAmt = 0.0f; h.dlyAmt = 0.0f; h.grnAmt = 1.0f;
          grain (h, 40.0f, 2.0f, 2.0f, 0.15f, 0.75f, 1.0f); h.grnSpread = 3.0f;
          p.dlyPing = 1.0f;
          delayFb (p, 0.5f, 1, 5000.0f);
      } },

    { "chaos_dead_battery", "CHAOS", "電池切れ", "全部が遅く暗く、グレインのピッチがゆっくり下がっていく。",
      50, 30, 50, 50,  0.25f, 0.25f, 0.25f,  50, 20, 60,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) { b.wahQ = 1.5f; b.wahLp = 0.7f; b.panSoft = 1.0f; grain (b, 120.0f, 2.0f, 1.0f, 0.25f, 0.25f, 0.5f); });
          wahRange (p.band[0], 80.0f, 300.0f);
          wahRange (p.band[1], 250.0f, 1200.0f);
          wahRange (p.band[2], 1500.0f, 4000.0f);
          delayFb (p, 0.55f, 1, 1200.0f);
          p.grnDrift = 12.0f; p.grnDriftBars = 6.0f;
          p.wetLP = 3500.0f;
      } },

    { "chaos_pulverize", "CHAOS", "粉砕", "全部最大。それでもPUNCHでビートは聞き取れる。",
      100, 100, 100, 100,  2.0f, 3.0f, 5.0f,  70, 80, 70,
      [] (PresetData& p) {
          forBands (p, [] (BandParams& b) {
              b.wahQ = 4.0f; b.wahShape = 1.0f; b.panSoft = 0.2f; b.panHaas = 10.0f;
              grain (b, 50.0f, 2.0f, 2.0f, 0.25f, 1.0f, 1.0f); b.grnSpread = 7.0f; b.grnRev = 0.3f;
          });
          p.dlyPing = 1.0f;
          delayFb (p, 0.7f, 1, 5000.0f);
          p.dlyRev = 0.3f;
      } },
};

PresetData build (const Def& d)
{
    PresetData p;
    p.wah = d.wah / 100.0f;
    p.pan = d.pan / 100.0f;
    p.dly = d.dly / 100.0f;
    p.grn = d.grn / 100.0f;
    p.recSpace = d.S / 100.0f;
    p.recChaos = d.C / 100.0f;
    p.recPunch = d.P / 100.0f;

    const float mul[3] = { d.mL, d.mM, d.mH };
    for (int b = 0; b < 3; ++b)
    {
        auto& bp = p.band[b];
        bp.mul = mul[b];
        bp.on = mul[b] > 0.0f ? 1.0f : 0.0f;
        bp.dlyMul = mul[b];
    }
    wahRange (p.band[0], 110.0f, 450.0f);
    wahRange (p.band[1], 350.0f, 2800.0f);
    wahRange (p.band[2], 2500.0f, 9500.0f);

    if (d.tweak != nullptr)
        d.tweak (p);
    return p;
}
} // namespace

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = []
    {
        std::vector<FactoryPreset> v;
        for (const auto& d : kDefs)
        {
            FactoryPreset f;
            f.id = d.id;
            f.category = d.cat;
            f.name = juce::String::fromUTF8 (d.name);
            f.description = juce::String::fromUTF8 (d.desc);
            f.data = build (d);
            v.push_back (f);
        }
        return v;
    }();
    return presets;
}

const juce::StringArray& getCategoryNames()
{
    static const juce::StringArray cats { "DRIFT", "ORBIT", "ECHO", "DUST", "CHAOS" };
    return cats;
}

int getDefaultPresetIndex()
{
    const auto& v = getFactoryPresets();
    for (int i = 0; i < (int) v.size(); ++i)
        if (v[(size_t) i].id == "orbit_seven_stars")
            return i;
    return 0;
}

} // namespace pon
