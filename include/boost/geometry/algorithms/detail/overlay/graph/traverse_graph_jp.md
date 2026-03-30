# traverse_graph.hpp — dissolve 対応の変更点

## 概要

グラフ走査中の **m_finished_tois チェック** と **走査完了後の toi 管理** に
`overlay_dissolve` 用の分岐を追加した。
dissolve では同一の turn operation (toi) が複数のリングで共有される場合があるため、
走査中の "通過" は許可しつつ、"起点としての再利用" は禁止する二重管理が必要になる。

## 変更箇所

### 1. continue_traverse() — m_finished_tois チェックの条件分岐

**オリジナル:**
```cpp
if (m_visited_tois.count(toi) > 0 || m_finished_tois.count(toi) > 0)
{
    return false;
}
```
走査済み (`m_visited_tois`) または完了済み (`m_finished_tois`) の toi に到達したら即座に中断。

**変更後:**
```cpp
if (m_visited_tois.count(toi) > 0)
{
    return false;
}
if constexpr (OverlayType != overlay_dissolve)
{
    if (m_finished_tois.count(toi) > 0)
    {
        return false;
    }
}
```
dissolve の場合、`m_finished_tois` のチェックをスキップする。
これにより、別のリングで既に使われた toi を **通過** できるようになる。

### 2. start_traverse() — リング完了後の toi 登録

**オリジナル:**
```cpp
m_finished_tois.insert(m_visited_tois.begin(), m_visited_tois.end());
```

**変更後:**
```cpp
if constexpr (OverlayType == overlay_dissolve)
{
    m_finished_tois.insert(m_visited_tois.begin(), m_visited_tois.end());
    m_all_traversed_tois.insert(m_visited_tois.begin(), m_visited_tois.end());
}
else
{
    m_finished_tois.insert(m_visited_tois.begin(), m_visited_tois.end());
}
```
dissolve では `m_all_traversed_tois` にも記録する。
`m_finished_tois` は **起点選択の排除** に使い、
`m_all_traversed_tois` は最終的な `update_administration()` に使う。

### 3. update_administration() — 参照先の切り替え

**オリジナル:**
```cpp
for (auto const& toi : m_finished_tois)
{
    auto& op = m_turns[toi.turn_index].operations[toi.operation_index];
    op.enriched.is_traversed = true;
}
```

**変更後:**
```cpp
auto const& tois_to_update = (OverlayType == overlay_dissolve)
    ? m_all_traversed_tois : m_finished_tois;
for (auto const& toi : tois_to_update)
{
    auto& op = m_turns[toi.turn_index].operations[toi.operation_index];
    op.enriched.is_traversed = true;
}
```
dissolve では `m_all_traversed_tois` を使って `is_traversed` を設定する。

### 4. メンバ変数の追加

```cpp
toi_set m_all_traversed_tois;
```
dissolve 専用。全リング走査で使われた toi の累積セット。

## 理由

dissolve では、自己交差ポリゴンの交点で複数のリングがエッジを **共有** する。
例えば、蝶ネクタイ型の交差点では交差点を通る toi が 2 つのリングで使われる。

オリジナルの union/intersection ロジックでは、完了済みの toi を通過しようとすると
即座に走査を中断するが、dissolve でこれをやると 2 番目のリングが生成できない。

そこで以下の二重管理を行う:

| セット | 起点選択 (`iterate`) | 走査中チェック (`continue_traverse`) |
|--------|---------------------|-------------------------------------|
| `m_finished_tois` | 除外する | **dissolve: スキップ** / 他: 除外する |
| `m_visited_tois` | — | 除外する（現在のリング内ループ防止） |
| `m_all_traversed_tois` | — | — （`update_administration` 専用） |

- `m_finished_tois`: 同じ toi から **新しいリングを開始** するのを防ぐ（重複リング防止）
- `m_visited_tois`: 現在のリング走査内でのループを防ぐ
- `m_all_traversed_tois`: 全走査の記録。`update_administration()` で `is_traversed = true` を設定する対象
