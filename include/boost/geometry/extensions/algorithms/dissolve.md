# Boost.Geometry Extension: dissolve

## Overview

`dissolve` removes self-intersections from a Polygon / Ring and produces a valid MultiPolygon. It is equivalent to "Union of a geometry with itself" in GIS terminology.

## Header

```cpp
#include <boost/geometry/extensions/algorithms/dissolve.hpp>
```

## Interface

```cpp
// OutputIterator version
template <typename GeometryOut, typename Geometry, typename OutputIterator>
OutputIterator dissolve_inserter(Geometry const& geometry, OutputIterator out);

// Collection version
template <typename Geometry, typename Collection>
void dissolve(Geometry const& geometry, Collection& output_collection);
```

### Usage Example

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
// output is a valid MultiPolygon without self-intersections
```

## Supported Geometries

| Input | Output |
|-------|--------|
| Ring | Ring |
| Polygon | Polygon |

## Algorithm

An implementation that specializes the Boost.Geometry overlay pipeline for self-intersections.

### Pipeline

```
Ring input
  |
  +- self_turns: detect self-intersection points (IPs)
  +- adapt_turns: convert all intersection operations to union
  +- enrich: assign cluster info and edge properties to IPs
  +- traverse: extract rings in overlay_dissolve mode
  +- select_rings + assign_parents: determine parent-child relationships
  +- add_rings: compose output rings
  +- dissolver: merge overlapping polygons
```

### overlay_dissolve Mode

Three modifications enable dissolve-specific behavior:

#### 1. select_edge.hpp — Left-Turn Preference

Normal union/intersection prefers right turns (tracing the exterior), but dissolve uses **left-turn preference**. This correctly traces the outer boundary of self-intersecting polygons.

```cpp
// Normal: Right(-1) first, Left(1) last -> right-turn preference
// Dissolve: Left(1) first, Right(-1) last -> left-turn preference
if constexpr (OverlayType == overlay_dissolve)
{
    return std::tie(b.side, a.toi) < std::tie(a.side, b.toi);
}
```

#### 2. traverse_graph.hpp — Separated TOI Management

In dissolve, finished operations (`m_finished_tois`) and all traversed operations (`m_all_traversed_tois`) are managed separately.

- **`m_finished_tois`**: prevents re-starting from the same operation (avoids duplicate rings)
- **`m_all_traversed_tois`**: accumulates all traversed operations (for `update_administration`)

In normal overlay, `m_finished_tois` alone serves both roles, but dissolve needs to share intersection points across multiple rings, so the `m_finished_tois` check is skipped in `continue_traverse`.

```cpp
if constexpr (OverlayType != overlay_dissolve)
{
    if (m_finished_tois.count(toi) > 0) { return false; }
}
```

#### 3. dissolve_traverse.hpp — Self-Overlay

When calling `traverse`, the same Geometry is passed as both arguments (self-overlay).

```cpp
detail::overlay::traverse
    <Reverse, Reverse, Geometry, Geometry, overlay_dissolve>
    ::apply(geometry, geometry, strategy, turns, rings, ...);
```

## Input Constraints

dissolve primarily targets cases where **the Polygon itself does not have self-intersecting Rings**.

- ✅ MultiPolygon with mutually intersecting Polygons → resolvable by dissolve
- ✅ Polygon with intersecting InnerRing and OuterRing → resolvable
- ⚠️ Self-intersecting Ring → result is subjective (depends on MakeValid strategy)

For self-intersecting Rings, use `make_valid` instead.

## Tests

```
geometry-develop/extensions/test/algorithms/dissolve.cpp
```

### Always-Enabled Tests

| Test Name | Description |
|-----------|-------------|
| dissolve_1 | Basic case (no self-intersection) |
| dissolve_d1, d2 | Donut shapes |
| dissolve_h1_a/b, h2, h3 | Polygon with holes |
| dissolve_mail_2017_09_24_b | Reported case |
| ggl_list_20110307_javier_01_a/b | Mailing list cases |
| multi_* (6 cases) | MultiPolygon cases |

### Conditional Tests (`BOOST_GEOMETRY_TEST_SELF_INTERSECTING_RINGS`)

Test cases with self-intersecting Rings are skipped by default.
The "correct answer" for these cases depends on the MakeValid strategy (non-zero winding rule vs even-odd fill rule).

To enable, define at compile time:
```
-DBOOST_GEOMETRY_TEST_SELF_INTERSECTING_RINGS
```

## Change History

### Modifications for Boost 1.90

| File | Change |
|------|--------|
| dissolve_traverse.hpp | Changed to use `overlay_dissolve` |
| select_edge.hpp | Added left-turn preference sort for dissolve |
| traverse_graph.hpp | Added dual management of `m_finished_tois` / `m_all_traversed_tois` |
| dissolve.cpp | Isolated self-intersecting Ring tests with `#if defined` |

## Related

- [make_valid](make_valid.md) — Decomposes self-intersecting Rings into a valid MultiPolygon
- `boost::geometry::is_valid` — Geometry validity check
- `boost::geometry::correct` — Fixes winding order and closure (does not handle self-intersections)
