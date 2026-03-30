# select_edge.hpp — dissolve 対応の変更点

## 概要

`select_by_side()` 内のエッジ選択ロジックに、`overlay_dissolve` 向けの分岐を追加した。
オリジナルは union/intersection 用に常に **右折優先** (Right first) だが、
dissolve では **左折優先** (Left first) に変更している。

## 変更箇所

### 1. Phase 1 ソート — side 値によるエッジ順序

**オリジナル（全 OverlayType 共通）:**
```cpp
std::sort(edges.begin(), edges.end(), [](auto const& a, auto const& b)
{
    return std::tie(a.side, a.toi) < std::tie(b.side, b.toi);
});
```
side = -1 (右) が先頭に来る。union/intersection では右折を選ぶのが正しい。

**変更後:**
```cpp
if constexpr (OverlayType == overlay_dissolve)
{
    // Left = 1 first, Right = -1 last (reverse of normal)
    std::sort(edges.begin(), edges.end(), [](auto const& a, auto const& b)
    {
        return std::tie(b.side, a.toi) < std::tie(a.side, b.toi);
    });
}
else
{
    std::sort(edges.begin(), edges.end(), [](auto const& a, auto const& b)
    {
        return std::tie(a.side, a.toi) < std::tie(b.side, b.toi);
    });
}
```
dissolve では `b.side` と `a.side` を入れ替えて降順にし、side = +1（左）が先頭に来るようにした。

### 2. Phase 2 — 同一 side 内の相互 side によるソート

**オリジナル:**
```cpp
int const side = side_strategy.apply(p2, a.point, b.point);
return side == 1;
```

**変更後:**
```cpp
int const side = side_strategy.apply(p2, a.point, b.point);
if constexpr (OverlayType == overlay_dissolve)
{
    return side == -1;
}
else
{
    return side == 1;
}
```
Phase 2 でも dissolve 時は左折優先の比較に反転させている。

## 理由

dissolve はポリゴンの自己交差を解消するアルゴリズムである。
ノードから複数のエッジが出る場合、**左折**（外側へ回る方向）を優先することで、
自己交差ポリゴンの外周境界を正しくトレースできる。
右折優先だと内側の小ループに入り込み、結果のリングが不正になる。
