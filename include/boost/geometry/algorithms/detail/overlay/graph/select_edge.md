# select_edge.hpp — Changes for dissolve Support

## Overview

Added `overlay_dissolve` branching to the edge selection logic in `select_by_side()`.
The original always uses **right-turn preference** (Right first) for union/intersection,
while dissolve uses **left-turn preference** (Left first).

## Changes

### 1. Phase 1 Sort — Edge Ordering by Side Value

**Original (common to all OverlayTypes):**
```cpp
std::sort(edges.begin(), edges.end(), [](auto const& a, auto const& b)
{
    return std::tie(a.side, a.toi) < std::tie(b.side, b.toi);
});
```
side = -1 (right) comes first. Right-turn selection is correct for union/intersection.

**Modified:**
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
For dissolve, `b.side` and `a.side` are swapped for descending order, placing side = +1 (left) first.

### 2. Phase 2 — Mutual Side Sort Within Same Side

**Original:**
```cpp
int const side = side_strategy.apply(p2, a.point, b.point);
return side == 1;
```

**Modified:**
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
Phase 2 comparison is also inverted to prefer left turns for dissolve.

## Rationale

dissolve is an algorithm for resolving polygon self-intersections.
When multiple edges leave a node, preferring **left turns** (outward direction)
correctly traces the outer boundary of self-intersecting polygons.
Right-turn preference would enter small inner loops, producing incorrect rings.
