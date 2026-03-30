// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2018 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_EXTENSIONS_ALGORITHMS_DISSOLVE_TRAVERSE_HPP
#define BOOST_GEOMETRY_EXTENSIONS_ALGORITHMS_DISSOLVE_TRAVERSE_HPP

#include <cstddef>

#include <boost/geometry/algorithms/detail/overlay/traverse.hpp>


namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace dissolve
{

template <bool Reverse, typename Backtrack>
class traverse
{

public :
    template
    <
        typename Geometry,
        typename IntersectionStrategy,
        typename Turns,
        typename Rings,
        typename TurnInfoMap,
        typename Clusters,
        typename Visitor
    >
    static inline void apply(Geometry const& geometry,
                IntersectionStrategy const& strategy,
                Turns& turns, Rings& rings,
                TurnInfoMap& turn_info_map,
                Clusters& clusters,
                Visitor& visitor)
    {
        boost::ignore_unused(visitor);

        // For dissolve, use overlay_dissolve which applies left-turn preference
        // to trace outer boundaries of self-intersecting polygons.
        detail::overlay::traverse
            <
                Reverse, Reverse,
                Geometry, Geometry,
                overlay_dissolve
            >::apply(geometry, geometry, strategy,
                     turns, rings, turn_info_map, clusters, visitor);
    }
};

}} // namespace detail::dissolve
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_EXTENSIONS_ALGORITHMS_DISSOLVE_TRAVERSE_HPP
