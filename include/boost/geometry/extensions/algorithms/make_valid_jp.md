# Boost.Geometry Extension: make_valid

## 概要

`make_valid` は、自己交差する Ring を持つ Polygon を、妥当な（valid な）MultiPolygon に変換するアルゴリズムです。PostGIS の `ST_MakeValid` に相当する機能を提供します。

## ヘッダ

```cpp
#include <boost/geometry/extensions/algorithms/make_valid.hpp>
```

## インタフェース

```cpp
template <typename Polygon, typename MultiPolygon>
void make_valid(Polygon const& input, MultiPolygon& output,
                make_valid_mode mode = make_valid_mode::basic);
```

### モード

| モード | 説明 | 塗りつぶしルール |
|--------|------|------------------|
| `make_valid_mode::basic` | 全フラグメントを外周として結合 | Non-zero winding |
| `make_valid_mode::strict` | 逆方向フラグメントを穴として扱う | Even-odd |

### 使用例

```cpp
#include <boost/geometry.hpp>
#include <boost/geometry/extensions/algorithms/make_valid.hpp>

namespace bg = boost::geometry;
using point = bg::model::d2::point_xy<double>;
using polygon = bg::model::polygon<point>;
using multi_polygon = bg::model::multi_polygon<polygon>;

// 五芒星（自己交差する Ring）
polygon pentagram;
bg::read_wkt("POLYGON((5 0,2.5 9,9.5 3.5,0.5 3.5,7.5 9,5 0))", pentagram);

// Basic mode: 星形全体を覆う 1 つのポリゴン
multi_polygon basic_result;
bg::make_valid(pentagram, basic_result, bg::make_valid_mode::basic);
// area = 25.6158, clips = 1

// Strict mode: 中央の五角形が穴、5 つの三角形のみ
multi_polygon strict_result;
bg::make_valid(pentagram, strict_result, bg::make_valid_mode::strict);
// area = 17.7317, clips = 5
```

---

## アルゴリズム詳細

自己交差する Ring を、**有向辺グラフ**と**右回り最小角度トレース**によって単純なリングに分解し、辺の方向フラグで分類するアルゴリズムです。

### 全体フロー

![アルゴリズム全体フロー](images/fig8_flow.png)

### Step 1: 自己交差点の検出

入力の Ring から、非隣接辺どうしの交差点（IP: Intersection Point）を検出します。
Boost.Geometry の `self_turns` API を使用し、すべての IP を一括取得します。

![Step 1: 自己交差する Ring（蝶ネクタイの例）](images/fig1_bowtie_input.png)

上図は蝶ネクタイ型の Ring `A(0,2) → B(2,4) → C(2,0) → D(4,2) → A` です。
辺 A→B と辺 C→D が IP(2,2) で交差しています。

### Step 2: 交差点での辺の分割

交差点の位置で辺を分割し、交差点が辺の端点となるようにします。
分割後は辺の数が増え、すべての交差が頂点上に集約されます。

![Step 2: 辺の分割](images/fig2_edge_splitting.png)

蝶ネクタイの例では、4辺 → 6辺に分割されます：
- A→B → **A→IP** + **IP→B** （交差点で分割）
- B→C → **B→C**（分割なし）
- C→D → **C→IP** + **IP→D** （交差点で分割）
- D→A → **D→A**（分割なし）

### Step 3: 双方向辺グラフの構築

分割された各辺（順方向）に対して、逆方向の辺を追加します。
これにより、任意の方向にトレース可能な**双方向辺グラフ**が構築されます。

![Step 3: 双方向辺グラフ](images/fig3_bidirectional.png)

各辺は**方向フラグ**を持ちます：
- **順方向（forward）**: 元の Ring と同じ方向の辺（direction = true）
- **逆方向（reverse）**: 双方向トレース用に追加した辺（direction = false）

この方向フラグが、後のフラグメント分類の鍵になります。

### Step 4: 右回り最小角度ルールによるリングトレース

双方向グラフに対して、**右回り最小角度**の辺を選び続けることで、単純なリングを抽出していきます。

![Step 4: 右回り最小角度選択](images/fig4_angle_selection.png)

#### トレースの規則

1. 任意の辺からスタートし、訪問済みフラグを付ける
2. 現在の辺の**終点**から出るすべての辺の中で、**右回り角度が最小の辺**を選ぶ
3. 未訪問辺を優先。すべて訪問済みなら訪問済み辺を使う（フォールバック）
4. 選んだ辺が既に訪問済みなら、**ループが閉じた** → そこまでの辺をリングとして抽出
5. 抽出後、すべての訪問済みフラグをリセットし、残りの辺で再開

#### 角度の計算

入射辺の始点 A、現在の頂点 B、候補辺の終点 C に対して、
ベクトル BA → BC の**時計回り角度**を `atan2` で計算します。
角度が 0° の場合は 360° として扱い、直進（折り返し）が最も大きい角度になります。

### Step 5: 辺の方向フラグによるフラグメント分類

抽出されたリングの各辺が持つ方向フラグにより、フラグメントの種類を判定します。

![Step 5: 方向フラグによる分類](images/fig7_direction_flags.png)

| 条件 | 分類 | 意味 |
|------|------|------|
| CW + 全辺が順方向 | **outer（外周）** | 元の Ring の進行方向に沿っている |
| CW + 全辺が逆方向 | **inner（穴）** | 元の Ring と完全に逆向き |
| CW + 混在 | **outer** | union により重複は解消される |
| CCW（反時計回り） | **outer（包絡線）** | Ring 全体を囲む境界 |

**直感的な理解**: 元の Ring を CW で描いたとき、「中が塗られる」部分を右回りトレースすると
全辺が順方向のフラグメントになります。「中が塗られない」部分は逆方向辺を使わないと
トレースできないため、全辺逆方向のフラグメントになります。

### 蝶ネクタイの分解例

![蝶ネクタイの分解結果](images/fig5_bowtie_result.png)

蝶ネクタイ `A(0,2)→B(2,4)→C(2,0)→D(4,2)→A` のトレース結果：

| フラグメント | 頂点 | 方向フラグ | 分類 |
|-------------|-------|-----------|------|
| 上三角形 | B→IP→A→B | 全辺順方向 | outer |
| 下三角形 | IP→D→C→IP | 全辺順方向 | outer |

→ 2つの valid な三角形に分解されます。

### 五芒星の分解例

![五芒星の分解と分類](images/fig6_pentagram.png)

五芒星 `(5,0)→(2.5,9)→(9.5,3.5)→(0.5,3.5)→(7.5,9)→(5,0)` のトレース結果：

| 分類 | 数 | 説明 |
|------|---|------|
| **全辺順方向** → outer | 5 | 星の5つの三角形 |
| **全辺逆方向** → inner | 1 | 中央の五角形 |
| **CCW 包絡線** → outer | 1 | 星形全体の外形 |

#### Basic mode の結果
union(5三角形 + 五角形 + 包絡線) = **星形全体** (area = 25.6)

#### Strict mode の結果
union(5三角形 + 包絡線) − union(五角形) = **5つの三角形のみ** (area = 17.7)

中央の五角形は「辺が全て逆方向」なので穴として扱われ、差し引かれます。

---

## 結果構築

```
split_ring(OuterRing) → outer フラグメント + inner フラグメント

split_ring(InnerRing) → inner の outer → 最終 inner
                       → inner の inner → 最終 outer（二重反転）

basic mode:  全フラグメント → outer として union
strict mode: union(outers) − union(inners) → 出力
```

`split_ring` はリング単体で自己完結する関数です。
Outer Ring と Inner Ring それぞれに独立して適用され、
結果を最上位の `make_valid` 関数で結合します。

---

## テスト結果

### Basic mode

| ケース | clips | area | valid | 備考 |
|--------|-------|------|-------|------|
| dissolve_3 (bowtie) | 2 | 4 | ✅ | 蝶ネクタイ → 2三角形 |
| dissolve_8 (pentagram) | 1 | 25.6158 | ✅ | 星形全体 |
| ggl_list_denis | 1 | 22544.2 | ✅ | 複雑な自己交差 |

### Strict mode

| ケース | clips | area | valid | 備考 |
|--------|-------|------|-------|------|
| dissolve_8 (pentagram) | 5 | 17.7317 | ✅ | 5三角形（中央は穴） |
| ggl_list_denis | 6 | 18677.3 | ✅ | 逆方向領域が穴 |

---

## dissolve との違い

| 項目 | dissolve | make_valid |
|------|----------|------------|
| 対象 | Polygon 間の重複除去 | Ring の自己交差修復 |
| 手法 | overlay traverse | 有向辺グラフ + 角度トレース |
| 分類 | overlay の ring selection | 辺の方向フラグ |
| モード | なし | basic / strict |

## 依存関係

- `self_turns` — 自己交差点の検出
- `union_` — フラグメントの結合
- `difference` — strict mode での穴の切り抜き
- `correct` — リングの巻き方向修正

## ファイル構成

```
extensions/algorithms/make_valid.hpp    ← 本体
src/test_make_valid.cpp                 ← テストドライバ
src/gen_make_valid_figures.py           ← 図の生成スクリプト
```

## 関連

- [dissolve](dissolve.md) — Polygon 間の重複除去（自己交差 Ring には非推奨）
- `boost::geometry::is_valid` — ジオメトリの妥当性チェック
- `boost::geometry::correct` — 巻き方向修正（自己交差は未対応）
- PostGIS `ST_MakeValid` — 同等機能の GIS 実装
