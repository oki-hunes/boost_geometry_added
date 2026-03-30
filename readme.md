# geometry_added — Summary of Changes

## Background

We had been using Boost.Geometry extensions (dissolve, offset, etc.),
so we applied modifications to make them work provisionally with Boost 1.90.

The removal of `no_rescale_policy` indicated that the internal algorithms
were being refactored, and we followed that direction in our modifications.

## dissolve Modifications

The dissolve test suite included various cases that appeared to have been
contributed via mailing list posts. However, many of these involved
self-intersecting rings, where the expected results seemed subjective and
ambiguous depending on the implementer's interpretation. We therefore
decided to exclude these cases from the tests.

Additionally, the dissolve tests using float precision did not pass, so
they were excluded from testing as well.

## make_valid Addition

The in-progress dissolve implementation appeared to incorporate functionality
equivalent to make_valid. However, we considered them to be fundamentally
different algorithms, so instead of relying on dissolve for this purpose,
we ported our in-house polygon reconstruction routine and provided it as
`make_valid` (we agree to the Boost Software License).

## File Listing

### New Files

| File | Description |
|------|-------------|
| `extensions/algorithms/make_valid.hpp` | make_valid implementation |
| `extensions/algorithms/make_valid.md` | make_valid documentation (English) |
| `extensions/algorithms/make_valid_jp.md` | make_valid documentation (Japanese) |
| `extensions/algorithms/dissolve.md` | dissolve documentation (English) |
| `extensions/algorithms/dissolve_jp.md` | dissolve documentation (Japanese) |
| `extensions/algorithms/images/fig1-fig8.png` | make_valid algorithm diagrams (8 files) |

### Modified Files

| File | Change |
|------|--------|
| `extensions/algorithms/dissolve.hpp` | Boost 1.90 API adaptation |
| `extensions/algorithms/offset.hpp` | Boost 1.90 API adaptation (no_rescale_policy removal, etc.) |
| `extensions/algorithms/detail/overlay/dissolver.hpp` | Boost 1.90 API adaptation |
| `extensions/algorithms/detail/overlay/dissolve_traverse.hpp` | overlay_dissolve mode support |
| `algorithms/detail/overlay/graph/select_edge.hpp` | Left-turn preference sort for dissolve |
| `algorithms/detail/overlay/graph/traverse_graph.hpp` | Dual TOI management for dissolve |
| `extensions/test/algorithms/dissolve.cpp` | Self-intersecting ring tests made conditional |
| `extensions/test/algorithms/Jamfile` | Boost 1.90 build adaptation |
| `test/geometry_test_common.hpp` | Boost 1.90 build adaptation |

### Change Documentation

| File | Subject |
|------|---------|
| `extensions/algorithms/offset.md` / `offset_jp.md` | offset.hpp changes |
| `algorithms/detail/overlay/graph/select_edge.md` / `select_edge_jp.md` | select_edge.hpp changes |
| `algorithms/detail/overlay/graph/traverse_graph.md` / `traverse_graph_jp.md` | traverse_graph.hpp changes |
