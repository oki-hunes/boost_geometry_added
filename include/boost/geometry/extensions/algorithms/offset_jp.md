# offset.hpp — Boost.Geometry 1.90 API 変更への追従

## 概要

Boost.Geometry 1.90 で buffer 関連の内部 API が変更されたことに伴い、
`offset.hpp` をコンパイルが通るように修正した。
アルゴリズム自体のロジック変更はなく、全て API 追従のための書き換えである。

## 変更箇所

### 1. #include の変更

**オリジナル:**
```cpp
#include <boost/geometry/strategies/agnostic/buffer_end_skip.hpp>
#include <boost/geometry/strategies/cartesian/buffer_side.hpp>
```
加えて `no_rescale_policy.hpp` を include していた。

**変更後:**
```cpp
#include <boost/geometry/strategies/buffer/cartesian.hpp>
#include <boost/geometry/strategies/cartesian/buffer_side_straight.hpp>
#include <boost/geometry/strategies/cartesian/buffer_end_flat.hpp>
```
1.90 で `buffer_end_skip` と `buffer_side` が削除/リネームされたため、
新しいヘッダに差し替えた。`no_rescale_policy.hpp` は不要になった。

### 2. `start_new_ring()` の引数追加

**オリジナル:**
```cpp
collection.start_new_ring();
```

**変更後:**
```cpp
collection.start_new_ring(false);
```
1.90 で `start_new_ring` に bool パラメータが必須になった。

### 3. `per_range::iterate()` のシグネチャ変更

**オリジナル:**
```cpp
per_range::iterate(collection, 0, boost::begin(range), boost::end(range), ...);
```

**変更後:**
```cpp
per_range::iterate(collection, boost::begin(range), boost::end(range), ..., true, ...);
```
第2引数の `0` (offset) が削除され、代わりに末尾付近に `true` (bool パラメータ) が追加された。

### 4. `buffered_piece_collection` のテンプレート引数変更

**オリジナル:**
```cpp
detail::buffer::buffered_piece_collection
    <
        model::ring<point_type>,
        detail::no_rescale_policy
    > collection(robust_policy);
```

**変更後:**
```cpp
using strategies_type = typename strategies::buffer::services::default_strategy
    <
        Geometry
    >::type;
strategies_type strategies;

detail::buffer::buffered_piece_collection
    <
        model::ring<point_type>,
        strategies_type,
        strategy::buffer::distance_asymmetric
            <
                typename geometry::coordinate_type<Geometry>::type
            >
    > collection(strategies, distance_strategy);
```
1.90 で rescale_policy が廃止され、strategies ベースの API に変わった。

### 5. end_strategy / side_strategy の型変更

**オリジナル:**
```cpp
strategy::buffer::end_skip<point_type, point_type> end_strategy;
strategy::buffer::buffer_side side_strategy;
```

**変更後:**
```cpp
strategy::buffer::end_flat end_strategy;
strategy::buffer::side_straight side_strategy;
```
`end_skip` が削除され `end_flat` に、`buffer_side` が `side_straight` にリネームされた。

### 6. `dispatch::offset::apply()` の最後の引数

**オリジナル:**
```cpp
::apply(collection, geometry, distance_strategy, side_strategy,
        join_strategy, end_strategy, robust_policy, reverse);
```

**変更後:**
```cpp
::apply(collection, geometry, distance_strategy, side_strategy,
        join_strategy, end_strategy, strategies, reverse);
```
`robust_policy` が `strategies` に置き換わった。

## 理由

Boost.Geometry 1.90 で buffer の内部実装が大幅にリファクタリングされ、
rescale_policy の廃止、strategy 名のリネーム、関数シグネチャの変更が行われた。
offset.hpp は extensions（非公式拡張）であるため、本体のリファクタリングに
自動的には追従されず、手動での修正が必要だった。
