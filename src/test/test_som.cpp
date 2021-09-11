
#include "catch.hpp"
#include "../som.hpp"


SCENARIO("The codebook is correctly created, initialized, and cleaned up")
{
  GIVEN("An initialized codebook")
  {
    CellIndexType width = 3;
    CellIndexType height = 4;
    IndexPointerType input_dim = 5;
    auto* codebook = new Codebook(width, height, input_dim, GlobalTopology::PLANE, LocalTopology::HEXA);
    codebook->init();
    THEN("All its values are between 0 and 1")
    {
      for (size_t i = 0; i < height * width * input_dim; ++i) {
        REQUIRE(0.0 <= codebook->get_value(i));
        REQUIRE(codebook->get_value(i) <= 1.0);
      }      
    }
  }
}


TEST_CASE("Saving and loading a codebook to file works")
{
  // Generate a temporary filename
  std::string filename = std::tmpnam(nullptr);

  // Create and save a codebook
  auto* codebook = new Codebook(2, 3, 4, GlobalTopology::PLANE, LocalTopology::CIRC);
  codebook->init();
  codebook->save_to_file(filename);
  Float v0 = codebook->get_value(0);
  Float v23 = codebook->get_value(23);
  delete codebook;

  // Create and load a codebook
  codebook = new Codebook(filename);
  REQUIRE(v0 == codebook->get_value(0));
  REQUIRE(v23 == codebook->get_value(23));
  delete codebook;
}
