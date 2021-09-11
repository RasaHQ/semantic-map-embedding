
#include "catch.hpp"
#include "../topo.hpp"

SCENARIO("Distance metrics satisfy the general metric properties")
{
  GIVEN("Any distance function on the plane or the torus")
  {

    DistanceFunction dist = GENERATE(
      as<DistanceFunction>{}, 
      distance_function(GlobalTopology::PLANE, LocalTopology::CIRC),
      distance_function(GlobalTopology::PLANE, LocalTopology::HEXA),
      distance_function(GlobalTopology::PLANE, LocalTopology::RECT),
      distance_function(GlobalTopology::TORUS, LocalTopology::CIRC),
      distance_function(GlobalTopology::TORUS, LocalTopology::HEXA),
      distance_function(GlobalTopology::TORUS, LocalTopology::RECT)
    );
    CellIndexType x1, y1, x2, y2, x3, y3, w, h;
    w = 4;
    h = 5;

    WHEN("Start and end points are identical")
    {
      x1 = x2 = GENERATE(0, 2, 4);
      y1 = y2 = GENERATE(0, 2, 4);

      THEN("The distance is zero")
      {
        REQUIRE(dist(x1, y1, x2, y2, w, h) == 0.);
      }
    }

    WHEN("Two points are randomly positioned")
    {
      x1 = GENERATE(0, 2, 4);
      y1 = GENERATE(0, 2, 4);
      x2 = GENERATE(0, 2, 4);
      y2 = GENERATE(0, 2, 4);

      THEN("The distance function is symmetric")
      {
        REQUIRE(
          dist(x1, y1, x2, y2, w, h) == 
          dist(x2, y2, x1, y1, w, h)
        );
      }
    }

    WHEN("Three points are randomly positioned")
    {
      x1 = GENERATE(0, 2, 3);
      y1 = GENERATE(0, 2, 3);
      x2 = GENERATE(0, 2, 3);
      y2 = GENERATE(0, 2, 3);
      x3 = GENERATE(0, 2, 3);
      y3 = GENERATE(0, 2, 3);

      THEN("The triangle inequality is satisfied")
      {
        REQUIRE(
          dist(x1, y1, x3, y3, w, h) <= 
          dist(x1, y1, x2, y2, w, h) + dist(x2, y2, x3, y3, w, h)
        );
      }
    }
  }
}


SCENARIO("Distances on the hexagonal grid are correct")
{
  GIVEN("A hexagonal distance function on the plane or the torus")
  {
    DistanceFunction dist = GENERATE(
      as<DistanceFunction>{}, 
      distance_function(GlobalTopology::PLANE, LocalTopology::HEXA),
      distance_function(GlobalTopology::TORUS, LocalTopology::HEXA)
    );

    WHEN("A cell is adjacent to (2, 2)")
    {
      THEN("Their distance is 1")
      {
        REQUIRE(dist(2, 2, 1, 1, 10, 10) == 1);  // Top left
        REQUIRE(dist(2, 2, 1, 2, 10, 10) == 1);  // Top right
        REQUIRE(dist(2, 2, 2, 1, 10, 10) == 1);  // Left
        REQUIRE(dist(2, 2, 2, 3, 10, 10) == 1);  // Right
        REQUIRE(dist(2, 2, 3, 1, 10, 10) == 1);  // Bottom left
        REQUIRE(dist(2, 2, 3, 2, 10, 10) == 1);  // Bottom right
      }
    }

    WHEN("A cell is adjacent to (3, 2)")
    {
      THEN("Their distance is 1")
      {
        REQUIRE(dist(3, 2, 2, 2, 10, 10) == 1);  // Top left
        REQUIRE(dist(3, 2, 2, 3, 10, 10) == 1);  // Top right
        REQUIRE(dist(3, 2, 3, 1, 10, 10) == 1);  // Left
        REQUIRE(dist(3, 2, 3, 3, 10, 10) == 1);  // Right
        REQUIRE(dist(3, 2, 4, 2, 10, 10) == 1);  // Bottom left
        REQUIRE(dist(3, 2, 4, 3, 10, 10) == 1);  // Bottom right
      }
    }
  }

  GIVEN("A hexagonal distance function on the plane")
  {
    DistanceFunction dist = distance_function(GlobalTopology::PLANE, LocalTopology::HEXA);

    WHEN("The map height and width are both 10")
    {
      THEN("The top left and bottom left cells have distance 10")
      {
        REQUIRE(dist(0, 0, 10, 0, 0, 0) == 10);
      }
      THEN("The top left and top right cells have distance 10")
      {
        REQUIRE(dist(0, 0, 0, 10, 0, 0) == 10);
      }
      THEN("The top left and bottom right cells have distance 15")
      {
        REQUIRE(dist(0, 0, 10, 10, 0, 0) == 15);
      }
    }
  }

  GIVEN("A hexagonal distance function on the torus")
  {
    DistanceFunction dist = distance_function(GlobalTopology::TORUS, LocalTopology::HEXA);

    WHEN("The map height and width are both 10")
    {
      THEN("The top left and bottom left cells have distance 1")
      {
        REQUIRE(dist(0, 0, 9, 0, 10, 10) == 1);
      }
      THEN("The top left and top right cells have distance 1")
      {
        REQUIRE(dist(0, 0, 0, 9, 10, 10) == 1);
      }
      THEN("The top left and bottom right cells have distance 1")
      {
        REQUIRE(dist(0, 0, 9, 9, 10, 10) == 1);
      }
    }
  }
}
