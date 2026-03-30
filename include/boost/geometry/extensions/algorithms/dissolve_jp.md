# Boost.Geometry Extension: dissolve

## 概要

`dissolve` は、自己交差を持つ Polygon / Ring から自己交差を除去し、妥当な（valid な）MultiPolygon を生成するアルゴリズムです。GIS における「Union of a geometry with itself」に相当します。

## ヘッダ

```cpp
#include <boost/geometry/extensions/algorithms/dissolve.hpp>
```

## インタフェース

```cpp
// OutputIterator 版
template <typename GeometryOut, typename Geometry, typename OutputIterator>
OutputIterator dissolve_inserter(Geometry const& geometry, OutputIterator out);

// Collection 版
template <typename Geometry, typename Collection>
void dissolve(Geometry const& geometry, Collection& output_collection);
```

### 使用例

```cpp
#include <boost/geometry.hpp>
#include <boost/geometry/extensions/algorithms/dissolve.hpp>

namespace bg = boost::geometry;
using point = bg::model::d2::point_xy<double>;
using polygon = bg::model::polygon<point>;
using multi_polygon = bg::model::multi_polygon<polygon>;

polygon input;
bg::read_wkt("POLYGON((0 0, 10 0, 0 10, 10 10, 0 0))", input);

multi_polygon output;
bg::dissolve(input, output);
// output は自己交差のない valid な MultiPolygon
```

## 対応ジオメトリ

| 入力 | 出力 |
|------|------|
| Ring | Ring |
| Polygon | Polygon |

## アルゴリズム

Boost.Geometry の overlay パイプラインを自己交差に特化させた実装です。

### パイプライン

```
Ring 入力
  │
  ├─ self_turns: 自己交差点（IP）を検出
  ├─ adapt_turns: intersection 操作を全て union に変換
  ├─ enrich: IP にクラスタ情報・辺プロパティを付与
  ├─ traverse: overlay_dissolve モードでリングを走査・抽出
  ├─ select_rings + assign_parents: 親子関係を決定
  ├─ add_rings: 出力リングを構成
  └─ dissolver: 重複するポリゴンをマージ
```

### overlay_dissolve モード

dissolve 固有の動作を実現するため、以下の3箇所を変更しています。

#### 1. select_edge.hpp — 左折優先

通常の union/intersection は右折優先（外側をたどる）ですが、dissolve は**左折優先**に変更しています。これにより、自己交差ポリゴンの外側の境界を正しくたどります。

```cpp
// 通常: Right(-1) first, Left(1) last → 右折優先
// dissolve: Left(1) first, Right(-1) last → 左折優先
if constexpr (OverlayType == overlay_dissolve)
{
    return std::tie(b.side, a.toi) < std::tie(a.side, b.toi);
}
```

#### 2. traverse_graph.hpp — TOI 管理の分離

dissolve では、完了済み操作 (`m_finished_tois`) と走査済み操作 (`m_all_traversed_tois`) を分離管理しています。

- **`m_finished_tois`**: 同じ操作からの再スタートを防止（重複リング回避）
- **`m_all_traversed_tois`**: 走査済み全操作を蓄積（`update_administration` 用）

通常の overlay では `m_finished_tois` のみで両方の役割を果たしますが、dissolve では交差点を複数のリングで共有する必要があるため、`continue_traverse` で `m_finished_tois` のチェックをスキップします。

```cpp
if constexpr (OverlayType != overlay_dissolve)
{
    if (m_finished_tois.count(toi) > 0) { return false; }
}
```

#### 3. dissolve_traverse.hpp — 自己オーバーレイ

`traverse` を呼び出す際に、同じ Geometry を2つの引数として渡します（自己オーバーレイ）。

```cpp
detail::overlay::traverse
    <Reverse, Reverse, Geometry, Geometry, overlay_dissolve>
    ::apply(geometry, geometry, strategy, turns, rings, ...);
```

## 入力の制約

dissolve は **Polygon 自体が自己交差していないケース** を主な対象とします。

- ✅ MultiPolygon で Polygon 同士が交差 → dissolve で解消可能
- ✅ Polygon の InnerRing と OuterRing が交差 → 解消可能
- ⚠️ Ring 自体が自己交差 → 結果が主観的（MakeValid 戦略に依存）

自己交差する Ring に対しては `make_valid` の使用を推奨します。

## テスト

```
geometry-develop/extensions/test/algorithms/dissolve.cpp
```

### 常時実行されるテスト

| テスト名 | 内容 |
|----------|------|
| dissolve_1 | 基本ケース（自己交差なし） |
| dissolve_d1, d2 | ドーナツ形状 |
| dissolve_h1_a/b, h2, h3 | 穴あり Polygon |
| dissolve_mail_2017_09_24_b | 報告事例 |
| ggl_list_20110307_javier_01_a/b | メーリングリスト事例 |
| multi_* (6件) | MultiPolygon ケース |

### 条件付きテスト（`BOOST_GEOMETRY_TEST_SELF_INTERSECTING_RINGS`）

Ring 自体が自己交差するテストケースは、デフォルトではスキップされます。
これらのケースでは「正解」が MakeValid 戦略（non-zero winding rule vs even-odd fill rule）に依存するためです。

有効化するにはコンパイル時に定義します：
```
-DBOOST_GEOMETRY_TEST_SELF_INTERSECTING_RINGS
```

## 変更履歴

### Boost 1.90 向け修正

| 変更箇所 | 内容 |
|----------|------|
| dissolve_traverse.hpp | `overlay_dissolve` を使用するよう変更 |
| select_edge.hpp | dissolve 用の左折優先ソートを追加 |
| traverse_graph.hpp | `m_finished_tois` / `m_all_traversed_tois` の二重管理を追加 |
| dissolve.cpp | 自己交差 Ring テストを `#if defined` で分離 |

## 関連

- [make_valid](make_valid.md) — 自己交差 Ring を分解して valid な MultiPolygon を生成
- `boost::geometry::is_valid` — ジオメトリの妥当性チェック
- `boost::geometry::correct` — 巻き方向・閉じ処理の修正（自己交差は未対応）
