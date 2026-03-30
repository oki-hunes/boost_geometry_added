# offset.hpp — Adaptation to Boost.Geometry 1.90 API Changes

## Overview

Modified `offset.hpp` to compile against Boost.Geometry 1.90, which changed
the buffer-related internal API. No algorithmic logic was changed — all
modifications are purely for API compatibility.

## Changes

### 1. #include Changes

**Original:**
```cpp
#include <boost/geometry/strategies/agnostic/buffer_end_skip.hpp>
#include <boost/geometry/strategies/cartesian/buffer_side.hpp>
```
Also included `no_rescale_policy.hpp`.

**Modified:**
```cpp
#include <boost/geometry/strategies/buffer/cartesian.hpp>
#include <boost/geometry/strategies/cartesian/buffer_side_straight.hpp>
#include <boost/geometry/strategies/cartesian/buffer_end_flat.hpp>
```
`buffer_end_skip` and `buffer_side` were removed/renamed in 1.90,
so headers were replaced with the new ones. `no_rescale_policy.hpp` is no longer needed.

### 2. `start_new_ring()` Argument Added

**Original:**
```cpp
collection.start_new_ring();
```

**Modified:**
```cpp
collection.start_new_ring(false);
```
A bool parameter became required for `start_new_ring` in 1.90.

### 3. `per_range::iterate()` Signature Change

**Original:**
```cpp
per_range::iterate(collection, 0, boost::begin(range), boost::end(range), ...);
```

**Modified:**
```cpp
per_range::iterate(collection, boost::begin(range), boost::end(range), ..., true, ...);
```
The second argument `0` (offset) was removed, and a `true` (bool parameter) was added near the end.

### 4. `buffered_piece_collection` Template Arguments Changed

**Original:**
```cpp
detail::buffer::buffered_piece_collection
    <
        model::ring<point_type>,
        detail::no_rescale_policy
    > collection(robust_policy);
```

**Modified:**
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
rescale_policy was removed in 1.90 and replaced with a strategies-based API.

### 5. end_strategy / side_strategy Type Changes

**Original:**
```cpp
strategy::buffer::end_skip<point_type, point_type> end_strategy;
strategy::buffer::buffer_side side_strategy;
```

**Modified:**
```cpp
strategy::buffer::end_flat end_strategy;
strategy::buffer::side_straight side_strategy;
```
`end_skip` was removed and replaced by `end_flat`; `buffer_side` was renamed to `side_straight`.

### 6. `dispatch::offset::apply()` Last Argument

**Original:**
```cpp
::apply(collection, geometry, distance_strategy, side_strategy,
        join_strategy, end_strategy, robust_policy, reverse);
```

**Modified:**
```cpp
::apply(collection, geometry, distance_strategy, side_strategy,
        join_strategy, end_strategy, strategies, reverse);
```
`robust_policy` was replaced with `strategies`.

## Rationale

Boost.Geometry 1.90 significantly refactored the buffer internals:
rescale_policy removal, strategy renaming, and function signature changes.
Since offset.hpp is part of extensions (unofficial), it was not automatically
updated by the main refactoring and required manual adaptation.
