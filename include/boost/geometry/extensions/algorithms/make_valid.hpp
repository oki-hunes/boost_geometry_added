// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2026 oki-tec

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_EXTENSIONS_ALGORITHMS_MAKE_VALID_HPP
#define BOOST_GEOMETRY_EXTENSIONS_ALGORITHMS_MAKE_VALID_HPP

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <vector>

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/size.hpp>
#include <boost/range/value_type.hpp>

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/union.hpp>
#include <boost/geometry/algorithms/difference.hpp>
#include <boost/geometry/algorithms/num_points.hpp>

#include <boost/geometry/algorithms/detail/overlay/self_turn_points.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_turn_info.hpp>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/exterior_ring.hpp>
#include <boost/geometry/core/interior_rings.hpp>
#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/geometries/multi_polygon.hpp>
#include <boost/geometry/geometries/concepts/check.hpp>


namespace boost { namespace geometry
{

enum class make_valid_mode
{
    basic,   // All fragments from outer ring -> outer; all from inner -> inner
    strict   // Additionally: reversed-direction outer fragments become inner
};


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace make_valid
{

// --- Directed edge structure ---
// Represents an edge from point s to point e, with a direction flag
// indicating whether it follows the original ring direction (true)
// or is a reverse edge added for bidirectional traversal (false).
template <typename CoordType>
struct directed_edge
{
    CoordType sx, sy, ex, ey;
    CoordType next_sx, next_sy, next_ex, next_ey; // link to next edge in chain
    bool direction; // true = original direction, false = reverse
    bool visited;   // used during ring traversal

    directed_edge()
        : sx(0), sy(0), ex(0), ey(0)
        , next_sx(0), next_sy(0), next_ex(0), next_ey(0)
        , direction(false), visited(false)
    {}

    directed_edge(CoordType sx_, CoordType sy_,
                  CoordType ex_, CoordType ey_, bool dir)
        : sx(sx_), sy(sy_), ex(ex_), ey(ey_)
        , next_sx(0), next_sy(0), next_ex(0), next_ey(0)
        , direction(dir), visited(false)
    {}

    // Sort by start point, then end point
    bool operator<(directed_edge const& r) const
    {
        if (sx != r.sx) return sx < r.sx;
        if (sy != r.sy) return sy < r.sy;
        if (ex != r.ex) return ex < r.ex;
        return ey < r.ey;
    }

    bool operator==(directed_edge const& r) const
    {
        return sx == r.sx && sy == r.sy && ex == r.ex && ey == r.ey;
    }
};


// --- No-interrupt policy for self_turns ---
struct no_interrupt_policy
{
    static bool const enabled = false;
    static bool const has_intersections = false;

    template <typename Range>
    static inline bool apply(Range const&)
    {
        return false;
    }
};


// Compute the angle A->B->C (clockwise angle from BA to BC, in range (0, 2*pi])
// 0 degrees is treated as 360 degrees.
template <typename CoordType>
inline double abc_angle(CoordType ax, CoordType ay,
                        CoordType bx, CoordType by,
                        CoordType cx, CoordType cy)
{
    double ba_x = static_cast<double>(ax) - static_cast<double>(bx);
    double ba_y = static_cast<double>(ay) - static_cast<double>(by);
    double bc_x = static_cast<double>(cx) - static_cast<double>(bx);
    double bc_y = static_cast<double>(cy) - static_cast<double>(by);

    double angle_ba = std::atan2(ba_y, ba_x);
    double angle_bc = std::atan2(bc_y, bc_x);

    // CW angle from BA to BC
    double angle = angle_ba - angle_bc;
    if (angle <= 0.0)
    {
        angle += 2.0 * 3.14159265358979323846;
    }
    return angle;
}


// Compute signed area of a polygon given as a vector of (x,y) pairs
template <typename CoordType>
inline double signed_area(std::vector<std::pair<CoordType, CoordType>> const& pts)
{
    double a = 0.0;
    std::size_t n = pts.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        std::size_t j = (i + 1) % n;
        a += static_cast<double>(pts[i].first) * static_cast<double>(pts[j].second);
        a -= static_cast<double>(pts[j].first) * static_cast<double>(pts[i].second);
    }
    return a * 0.5;
}


// Split a ring into simple sub-rings using directed edge graph traversal:
//   1. Detect all self-intersection points using self_turns
//   2. Split edges at intersection points to create directed edges
//   3. Add reverse edges for bidirectional graph
//   4. Trace rings using right-turn minimum angle rule
//   5. Classify each ring by its edge direction flags
//
// Output: outers (all-forward or mixed edges), inners (all-reverse edges)
template <typename Ring, typename Strategy>
void split_ring(Ring const& input,
                std::deque<Ring>& outers,
                std::deque<Ring>& inners,
                Strategy const& strategy)
{
    using point_type = typename geometry::point_type<Ring>::type;
    using coord_type = typename geometry::coordinate_type<Ring>::type;
    using edge_type = directed_edge<coord_type>;

    std::size_t const n = boost::size(input);
    if (n < 4) return;

    // --- Step 1: Get all edges from the ring ---
    std::vector<edge_type> dv_vector;
    dv_vector.reserve(n);
    for (std::size_t i = 0; i < n - 1; ++i)
    {
        auto const& p0 = *(boost::begin(input) + i);
        auto const& p1 = *(boost::begin(input) + i + 1);
        coord_type x0 = geometry::get<0>(p0);
        coord_type y0 = geometry::get<1>(p0);
        coord_type x1 = geometry::get<0>(p1);
        coord_type y1 = geometry::get<1>(p1);
        if (x0 == x1 && y0 == y1) continue;
        dv_vector.push_back(edge_type(x0, y0, x1, y1, true));
    }

    if (dv_vector.empty()) return;

    // --- Step 2: Find self-intersections using self_turns ---
    using ratio_type = boost::geometry::segment_ratio
        <typename geometry::coordinate_type<Ring>::type>;
    using turn_info_type = detail::overlay::turn_info<point_type, ratio_type>;

    std::deque<turn_info_type> turns;
    no_interrupt_policy policy;

    detail::self_get_turn_points::self_turns
        <false, detail::overlay::assign_policy_only_start_turns>
        (input, strategy, turns, policy, 0, true);

    if (turns.empty())
    {
        outers.push_back(input);
        return;
    }

    // --- Step 3: Split edges at intersection points ---
    // For each pair of crossing segments,
    // split both into two sub-edges at the intersection point.
    // We use self_turns which gives us segment indices and the IP.

    // Collect split points per original segment
    using split_info = std::pair<double, std::pair<coord_type, coord_type>>;
    std::map<int, std::vector<split_info>> split_points;

    for (auto const& turn : turns)
    {
        coord_type ix = geometry::get<0>(turn.point);
        coord_type iy = geometry::get<1>(turn.point);

        for (int op = 0; op < 2; ++op)
        {
            int seg_idx = turn.operations[op].seg_id.segment_index;
            auto const& p0 = *(boost::begin(input) + seg_idx);
            auto const& p1 = *(boost::begin(input) + seg_idx + 1);
            double dx = static_cast<double>(geometry::get<0>(p1))
                      - static_cast<double>(geometry::get<0>(p0));
            double dy = static_cast<double>(geometry::get<1>(p1))
                      - static_cast<double>(geometry::get<1>(p0));
            double frac = 0.0;
            if (std::abs(dx) > std::abs(dy))
                frac = (static_cast<double>(ix) - static_cast<double>(geometry::get<0>(p0))) / dx;
            else if (std::abs(dy) > 0.0)
                frac = (static_cast<double>(iy) - static_cast<double>(geometry::get<1>(p0))) / dy;

            split_points[seg_idx].push_back(
                std::make_pair(frac, std::make_pair(ix, iy)));
        }
    }

    for (auto& sp : split_points)
    {
        std::sort(sp.second.begin(), sp.second.end());
        sp.second.erase(
            std::unique(sp.second.begin(), sp.second.end(),
                [](split_info const& a, split_info const& b)
                { return a.second == b.second; }),
            sp.second.end());
    }

    // Build split edge list
    std::vector<edge_type> split_edges;
    split_edges.reserve(dv_vector.size() + turns.size() * 2);

    for (std::size_t i = 0; i < n - 1; ++i)
    {
        auto const& p0 = *(boost::begin(input) + i);
        auto const& p1 = *(boost::begin(input) + i + 1);
        coord_type x0 = geometry::get<0>(p0);
        coord_type y0 = geometry::get<1>(p0);
        coord_type x1 = geometry::get<0>(p1);
        coord_type y1 = geometry::get<1>(p1);
        if (x0 == x1 && y0 == y1) continue;

        auto it = split_points.find(static_cast<int>(i));
        if (it == split_points.end())
        {
            split_edges.push_back(edge_type(x0, y0, x1, y1, true));
        }
        else
        {
            coord_type cx = x0, cy = y0;
            for (auto const& sp : it->second)
            {
                coord_type ix = sp.second.first;
                coord_type iy = sp.second.second;
                if (ix == cx && iy == cy) continue;
                split_edges.push_back(edge_type(cx, cy, ix, iy, true));
                cx = ix; cy = iy;
            }
            if (cx != x1 || cy != y1)
                split_edges.push_back(edge_type(cx, cy, x1, y1, true));
        }
    }

    // --- Step 4: Build bidirectional edge set ---
    // Insert all forward edges, then for each,
    // insert reverse edge (direction=false) if not already present.
    std::multiset<edge_type> edge_set;
    for (auto const& e : split_edges)
        edge_set.insert(e);

    for (auto const& e : split_edges)
    {
        edge_type rev(e.ex, e.ey, e.sx, e.sy, false);
        if (edge_set.find(rev) == edge_set.end())
            edge_set.insert(rev);
    }

    // --- Step 5: Ring traversal ---
    // Use visited flag, don't erase edges during trace.
    // When a visited edge is reached, extract the sub-ring.

    // Helper: find next edge using minimum CW angle, preferring unvisited
    auto get_next_chain = [&](coord_type sx, coord_type sy,
                              coord_type ex, coord_type ey)
        -> typename std::multiset<edge_type>::iterator
    {
        edge_type search_key;
        search_key.sx = ex; search_key.sy = ey;
        search_key.ex = std::numeric_limits<coord_type>::lowest();
        search_key.ey = std::numeric_limits<coord_type>::lowest();

        auto lo = edge_set.lower_bound(search_key);

        double min_angle = 1e30;
        double fallback_angle = 1e30;
        auto best = edge_set.end();
        auto fallback = edge_set.end();

        for (auto it = lo; it != edge_set.end()
             && it->sx == ex && it->sy == ey; ++it)
        {
            double angle = abc_angle(sx, sy, ex, ey, it->ex, it->ey);
            if (!it->visited)
            {
                if (angle < min_angle)
                {
                    min_angle = angle;
                    best = it;
                }
            }
            else
            {
                if (angle < fallback_angle)
                {
                    fallback_angle = angle;
                    fallback = it;
                }
            }
        }
        if (best != edge_set.end()) return best;
        return fallback;
    };

    // Helper: reset all visited flags
    auto reset_visited = [&]()
    {
        for (auto it = edge_set.begin(); it != edge_set.end(); ++it)
            const_cast<edge_type&>(*it).visited = false;
    };

    // Helper: extract ring starting from 'start_it', following next_s/next_e chain
    auto take_chain = [&](typename std::multiset<edge_type>::iterator start_it)
    {
        // Collect edges in the ring by following the chain
        struct chain_entry
        {
            coord_type sx, sy, ex, ey;
            coord_type next_sx, next_sy, next_ex, next_ey;
            bool direction;
        };

        std::vector<chain_entry> chain;
        auto it = start_it;
        while (it != edge_set.end())
        {
            chain_entry ce;
            ce.sx = it->sx; ce.sy = it->sy;
            ce.ex = it->ex; ce.ey = it->ey;
            ce.next_sx = it->next_sx; ce.next_sy = it->next_sy;
            ce.next_ex = it->next_ex; ce.next_ey = it->next_ey;
            ce.direction = it->direction;
            chain.push_back(ce);

            edge_set.erase(it);

            // Follow the recorded next edge
            edge_type find_key(ce.next_sx, ce.next_sy, ce.next_ex, ce.next_ey, false);
            find_key.direction = false; // don't care about direction for find
            // Find exact match
            it = edge_set.end();
            for (auto fi = edge_set.lower_bound(edge_type(ce.next_sx, ce.next_sy,
                     std::numeric_limits<coord_type>::lowest(),
                     std::numeric_limits<coord_type>::lowest(), false));
                 fi != edge_set.end() && fi->sx == ce.next_sx && fi->sy == ce.next_sy;
                 ++fi)
            {
                if (fi->ex == ce.next_ex && fi->ey == ce.next_ey)
                {
                    it = fi;
                    break;
                }
            }
        }

        // Build ring from chain
        int forward_count = 0;
        std::vector<std::pair<coord_type, coord_type>> pts;
        for (auto const& ce : chain)
        {
            pts.push_back(std::make_pair(ce.sx, ce.sy));
            if (ce.direction) ++forward_count;
        }

        if (pts.size() < 3) return;

        double area = signed_area(pts);
        if (std::abs(area) < 1e-10) return;

        // Build Ring geometry
        Ring r;
        for (auto const& pt : pts)
        {
            point_type p;
            geometry::set<0>(p, pt.first);
            geometry::set<1>(p, pt.second);
            r.push_back(p);
        }
        // Close
        {
            point_type p;
            geometry::set<0>(p, pts.front().first);
            geometry::set<1>(p, pts.front().second);
            r.push_back(p);
        }

        int total_count = static_cast<int>(chain.size());

        if (area > 0)
        {
            // CW ring (shoelace positive = CW in screen coords)
            if (forward_count == total_count)
            {
                // All forward -> outer polygon
                outers.push_back(std::move(r));
            }
            else if (forward_count == 0)
            {
                // All reverse -> hole
                inners.push_back(std::move(r));
            }
            else
            {
                // Mixed direction -> outer
                // Union resolves any overlaps with all-forward fragments
                outers.push_back(std::move(r));
            }
        }
        else
        {
            // CCW ring = envelope (bounding outline)
            // For MultiPolygon support, keep all.
            outers.push_back(std::move(r));
        }
    };

    // Main ring traversal loop
    while (!edge_set.empty())
    {
        auto last_it = edge_set.begin();

        while (true)
        {
            // Mark as visited
            const_cast<edge_type&>(*last_it).visited = true;

            // Get next chain edge
            auto next_it = get_next_chain(last_it->sx, last_it->sy,
                                          last_it->ex, last_it->ey);

            if (next_it == edge_set.end())
            {
                // Dead end - remove this edge and restart
                edge_set.erase(last_it);
                reset_visited();
                break;
            }

            if (next_it->visited)
            {
                // Chain is complete - record the closing edge link
                const_cast<edge_type&>(*last_it).next_sx = next_it->sx;
                const_cast<edge_type&>(*last_it).next_sy = next_it->sy;
                const_cast<edge_type&>(*last_it).next_ex = next_it->ex;
                const_cast<edge_type&>(*last_it).next_ey = next_it->ey;

                // Extract ring starting from next_it
                take_chain(next_it);

                reset_visited();
                break;
            }

            // Record next edge link
            const_cast<edge_type&>(*last_it).next_sx = next_it->sx;
            const_cast<edge_type&>(*last_it).next_sy = next_it->sy;
            const_cast<edge_type&>(*last_it).next_ex = next_it->ex;
            const_cast<edge_type&>(*last_it).next_ey = next_it->ey;

            last_it = next_it;
        }
    }
}


// Union a collection of rings into a MultiPolygon
template <typename Ring, typename MultiPolygon>
void union_rings(std::deque<Ring>& ring_fragments, MultiPolygon& result)
{
    using polygon_type = typename boost::range_value<MultiPolygon>::type;

    for (auto& frag : ring_fragments)
    {
        geometry::correct(frag);

        polygon_type p;
        geometry::exterior_ring(p) = frag;

        if (result.empty())
        {
            result.push_back(p);
        }
        else
        {
            MultiPolygon temp;
            geometry::union_(result, p, temp);
            result = std::move(temp);
        }
    }
}


}} // namespace detail::make_valid
#endif // DOXYGEN_NO_DETAIL


// make_valid: converts a potentially self-intersecting Polygon
// into a valid MultiPolygon.
//
// Algorithm (directed edge graph traversal):
//   1. Extract edges from outer ring
//   2. Find all self-intersection points (via self_turns)
//   3. Split edges at intersection points
//   4. Add reverse edges for bidirectional graph
//   5. Trace simple rings via right-turn minimum angle rule
//   6. Classify by edge direction: all-forward -> outer, all-reverse -> hole
//   7. Same for inner rings
//   8. Result = union(outer fragments) - union(inner fragments)
//
// Modes:
//   basic  - only all-reverse-edge fragments become holes
//   strict - same as basic (direction-based classification is inherent)
template <typename Polygon, typename MultiPolygon>
inline void make_valid(Polygon const& input, MultiPolygon& output,
                       make_valid_mode mode = make_valid_mode::basic)
{
    concepts::check<Polygon const>();

    using ring_type = typename geometry::ring_type<Polygon>::type;
    using strategy_type = typename strategies::relate::services::default_strategy
        <
            Polygon, Polygon
        >::type;

    strategy_type strategy;

    // Step 1: Split outer ring
    std::deque<ring_type> outer_outers;
    std::deque<ring_type> outer_inners;
    detail::make_valid::split_ring(
        geometry::exterior_ring(input), outer_outers, outer_inners, strategy);

    // Step 2: Split inner rings
    std::deque<ring_type> inner_parts;
    {
        auto const& rings = geometry::interior_rings(input);
        for (auto it = boost::begin(rings); it != boost::end(rings); ++it)
        {
            std::deque<ring_type> io, ii;
            detail::make_valid::split_ring(*it, io, ii, strategy);
            // Inner ring fragments: outers from inner ring -> become inners
            // (inner ring outers = reversed-outer = holes in the polygon)
            for (auto& r : io)
            {
                outer_inners.push_back(std::move(r));
            }
            // Inner ring inners -> stay as outer
            // (doubly-reversed = original direction = outer)
            for (auto& r : ii)
            {
                outer_outers.push_back(std::move(r));
            }
        }
    }

    // In basic mode, ignore direction-based inner classification
    // (treat all fragments from outer ring as outer)
    if (mode == make_valid_mode::basic)
    {
        for (auto& r : outer_inners)
        {
            outer_outers.push_back(std::move(r));
        }
        outer_inners.clear();
    }

    // Step 3: Union outer fragments
    MultiPolygon outer_union;
    detail::make_valid::union_rings(outer_outers, outer_union);

    if (outer_inners.empty())
    {
        output = std::move(outer_union);
        return;
    }

    // Step 4: Union inner fragments
    MultiPolygon inner_union;
    detail::make_valid::union_rings(outer_inners, inner_union);

    // Step 5: Result = outer - inner
    geometry::difference(outer_union, inner_union, output);
}


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_EXTENSIONS_ALGORITHMS_MAKE_VALID_HPP
