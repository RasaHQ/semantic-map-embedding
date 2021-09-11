
#include <stdexcept>      // std::invalid_argument
#include <assert.h>
#include "topo.hpp"
#include "utils.hpp"


// Specific distance functions ////////////////////////////////////////////////////


inline CellIndexType dist_circle_plane(int y, int x, int i, int j, int, int)
{
    return std::ceil(std::sqrt(squared(std::abs(i - y)) + squared(std::abs(j - x))));
}


inline CellIndexType dist_circle_torus(int y, int x, int i, int j, int height, int width)
{
  assert (0 <= x);
  assert (0 <= y);
  assert (x <= width);
  assert (y <= height);
  
  int dx = abs(j - x);
	int dy = abs(i - y);

  return std::ceil(std::sqrt(squared(std::min(dx, width - dx)) + squared(std::min(dy, height - dy))));
}


// Hexagonal lattice layout with 'pointy top' and shifting odd rows by 1/2
// See https://www.redblobgames.com/grids/hexagons/
// Here we simplified the expression with Mathematica
inline CellIndexType dist_hexa_plane(int row1, int col1, int row2, int col2, int, int)
{
  CellIndexType a = std::abs(row1 - row2);
  CellIndexType b = std::abs(col1 - col2 - (row1 - (row1 & 1)) / 2 + (row2 - (row2 & 1)) / 2);
  CellIndexType c = std::abs(col1 - col2 + row1 - row2 - (row1 - (row1 & 1)) / 2 + (row2 - (row2 & 1)) / 2);
  return std::max({a, b, c});
}


inline CellIndexType dist_hexa_torus(int row1, int col1, int row2, int col2, int height, int width)
{
  CellIndexType a = dist_hexa_plane(row1, col1, row2, col2, 0, 0);

  CellIndexType b = dist_hexa_plane(row1, col1, row2 + height, col2, 0, 0);
  CellIndexType c = dist_hexa_plane(row1, col1, row2, col2 + width, 0, 0);
  CellIndexType d = dist_hexa_plane(row1, col1, row2 + height, col2 + width, 0, 0);

  CellIndexType e = dist_hexa_plane(row1 + height, col1, row2, col2, 0, 0);
  CellIndexType f = dist_hexa_plane(row1, col1 + width, row2, col2, 0, 0);
  CellIndexType g = dist_hexa_plane(row1 + height, col1 + width, row2, col2, 0, 0);
  
  return std::min({a, b, c, d, e, f, g});
}


inline CellIndexType dist_rect_plane(int y, int x, int i, int j, int, int)
{
    return std::max(std::abs(i - y), std::abs(j - x));
}


inline CellIndexType dist_rect_torus(int y, int x, int i, int j, int height, int width)
{
  double dx = abs(j - x);
	double dy = abs(i - y);
  dx = std::min(dx, width - dx);
  dy = std::min(dy, height - dy);
  return std::max(dx, dy);
}


// Implementation of distance_function ////////////////////////////////////////////


DistanceFunction distance_function(GlobalTopology global_topology, LocalTopology local_topology)
{
  switch (global_topology)
  {
  case GlobalTopology::PLANE:
    switch (local_topology)
    {
    case LocalTopology::CIRC:
      return dist_circle_plane;
      break;
    case LocalTopology::HEXA:
      return dist_hexa_plane;
      break;
    case LocalTopology::RECT:
      return dist_rect_plane;
    default:
      throw std::invalid_argument("Invalid topology specification");
      break;
    }
    break;
  case GlobalTopology::TORUS:
    switch (local_topology)
    {
    case LocalTopology::CIRC:
      return dist_circle_torus;
      break;
    case LocalTopology::HEXA:
      return dist_hexa_torus;
      break;
    case LocalTopology::RECT:
      return dist_rect_torus;
      break;
    default:
      throw std::invalid_argument("Invalid topology specification");
      break;
    }
    break;  
  default:
    throw std::invalid_argument("Invalid topology specification");
    break;
  }
}
