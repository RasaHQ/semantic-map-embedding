
#pragma once

#include <cmath>
#include "data.hpp"

#define HEXAGON_R 0.8660254037844386  // Maximal hexagon radius = sqrt(3) / 2

enum LocalTopology
{
    RECT=8, HEXA=6, CIRC=4
};

inline std::string get_local_topology_string(LocalTopology local_topology)
{
    switch (local_topology)
    {
    case LocalTopology::RECT:
        return "rectangular (8 neighbours)";
        break;
    case LocalTopology::HEXA:
        return "hexagonal (6 neighbours)";
    case LocalTopology::CIRC:
        return "circular (4 neighbours)";
    default:
        return "UNKNOWN";
        break;
    }
}

enum GlobalTopology
{
    TORUS=0, MOEBIUS=1, TUBE=2, PLANE=4
};

inline std::string get_global_topology_string(GlobalTopology global_topology)
{
    switch (global_topology)
    {
    case GlobalTopology::TORUS:
        return "torus (connecting east/west and north/south)";
        break;
    case GlobalTopology::MOEBIUS:
        return "moebius (connecting east/west with one twist)";
        break;
    case GlobalTopology::TUBE:
        return "tube (connecting east/west)";
        break;
    case GlobalTopology::PLANE:
        return "plane";
        break;
    default:
        return "UNKNOWN";
        break;
    }
}

typedef CellIndexType (*DistanceFunction) (int, int, int, int, int, int);

DistanceFunction distance_function(GlobalTopology global_topology, LocalTopology local_topology);
