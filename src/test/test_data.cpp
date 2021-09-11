
#include "catch.hpp"
#include "../data.hpp"


TEST_CASE("Dummy data loads without problems")
{
  CorpusDataset* dataset;
  if (file_exists("data/dummy.bin")) {
    dataset = new CorpusDataset("data/dummy.bin");
  } else if (file_exists("../data/dummy.bin")) {
    dataset = new CorpusDataset("../data/dummy.bin");
  } else {
    WARN("Skipping test since 'dummy.bin' not found");
    return;
  }
  REQUIRE( dataset->num_rows == 8 );
  REQUIRE( dataset->num_cols == 12 );
}
