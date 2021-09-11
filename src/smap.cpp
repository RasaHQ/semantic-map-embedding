
#include <iostream>
#include <fstream>
#include <algorithm>  // fill_n
#include <assert.h>
#include <exception>
#include "smap.hpp"


SemanticMap::SemanticMap() :
  counts(nullptr),
  best_matching_units(nullptr),
  vocabulary(nullptr),
  vocabulary_size(0)
{}


SemanticMap::SemanticMap(const std::string& counts_filename) :
  counts(nullptr),
  best_matching_units(nullptr),
  vocabulary(nullptr),
  vocabulary_size(0),
  should_cleanup_best_matching_units(true)
{
  this->load_counts_from_file(counts_filename);
}


SemanticMap::SemanticMap(const std::string& counts_filename, const std::string& best_matching_units_filename) :
  counts(nullptr),
  best_matching_units(nullptr),
  vocabulary(nullptr),
  should_cleanup_best_matching_units(true)
{
  std::cout << "Loading semantic map data" << std::endl;
  this->load_counts_from_file(counts_filename);
  this->load_best_matching_units_from_file(best_matching_units_filename);
}


SemanticMap::SemanticMap(const CorpusDataset& data, const Codebook& codebook, const IndexType train_vocab_cutoff) :
  counts(nullptr),
  best_matching_units(nullptr),
  vocabulary(nullptr),
  should_cleanup_best_matching_units(true)
{
  this->build(data, codebook, train_vocab_cutoff);
}


SemanticMap::~SemanticMap()
{
  if (this->counts)
  {
    delete [] this->counts;
    this->counts = nullptr;
  }
  if (this->best_matching_units && this->should_cleanup_best_matching_units)
  {
    delete [] this->best_matching_units;
    this->best_matching_units = nullptr;
  }
  if (this->vocabulary)
  {
    delete [] this->vocabulary;
    this->vocabulary = nullptr;
  }
}


void SemanticMap::delete_counts()
{
  if (this->counts)
  {
    delete [] this->counts;
    this->counts = nullptr;
  }
}


void SemanticMap::build(const CorpusDataset& data, CellIndexType* best_matching_units, const CellIndexType _height, const CellIndexType _width)
{
  assert (!this->counts);

  std::cout << "Creating semantic map" << std::endl;

  this->vocabulary_size = data.num_cols;
  this->dataset_size = data.num_rows;
  this->num_cells = _height * _width;
  this->height = _height;
  this->width = _width;

  this->best_matching_units = best_matching_units;
  this->should_cleanup_best_matching_units = false;

  this->build_counts(data);
}


void SemanticMap::build(const CorpusDataset& data, const Codebook& codebook, IndexType train_vocab_cutoff)
{
  assert (!this->counts);
  assert (data.num_cols == codebook.get_input_dim());

  const auto effective_input_dim = (train_vocab_cutoff > 0 ? train_vocab_cutoff : data.num_cols);

  std::cout << "Creating semantic map" << std::endl;

  this->vocabulary_size = data.num_cols;
  this->dataset_size = data.num_rows;
  this->num_cells = codebook.get_num_cells();
  this->height = codebook.get_height();
  this->width = codebook.get_width();

  // Get cells that correspond to snippets
  std::cout << "  Find best matching units" << std::endl;
  this->best_matching_units = new CellIndexType[data.num_rows];
  auto* distances = new Float[data.num_rows];
  codebook.find_best_matching_units(data, this->best_matching_units, distances, effective_input_dim, false);
  delete [] distances;

  this->build_counts(data);
}


void SemanticMap::build_counts(const CorpusDataset& data)
{
  // Create and initialize int array with one entry for each cell in the map and each term in the vocabulary
  if (!this->counts)
    this->counts = new CountType[this->num_cells * this->vocabulary_size];
  std::fill_n(this->counts, this->num_cells * this->vocabulary_size, 0);

  // For each word in each snippet, add 1 to the cell that corresponds to this word (word index) and this snippet (best_matching_unit)
  std::cout << "  Count associations" << std::endl;
  for (size_t row = 0; row < data.num_rows; ++row)
  {
    const CellIndexType best_matching_unit = this->best_matching_units[row];
    assert (best_matching_unit < num_cells);

    const IndexType* const indices = data.indices_in_row(row);     // Address of the beginning of the array of columns-with-value-1-indices
    const IndexType num_non_zero_in_row = data.num_indices_in_row(row);

    for (IndexType i = 0; i < num_non_zero_in_row; ++i) {
      const IndexType vocab_index = indices[i];
      if (this->counts[this->num_cells * vocab_index + best_matching_unit] >= MAX_COUNT - 1)
      {
        std::cerr << "Exceding MAX_COUNT of " << MAX_COUNT << std::endl;  // ToDo: Implement solution to MAX_COUNT excess
        this->delete_counts();
        return;
      }
      this->counts[this->num_cells * vocab_index + best_matching_unit] += 1;
    }
  }
}


std::vector<size_t> SemanticMap::find_snippets(const CorpusDataset& data, CellIndexType map_row, CellIndexType map_col)
{
  assert (this->best_matching_units);
  assert (this->width > 0);

  std::cout << "Find snippets associated with cell " << map_row+1 << "/" << map_col+1 << std::endl;
  std::vector<size_t> associated_snippets = {};
  for (IndexPointerType snippet_index = 0; snippet_index < data.num_rows; ++snippet_index)
  {
    const CellIndexType i = best_matching_units[snippet_index] / this->width,
                        j = best_matching_units[snippet_index] % this->width;

    if (i == map_row && j == map_col)
      associated_snippets.push_back(snippet_index);
  }

  return associated_snippets;
}


void SemanticMap::associate_vocabulary(const std::string& filename)
{
  if (this->vocabulary)
  {
    std::cout << "WARNING: Replacing vocabulary" << std::endl;
    delete [] this->vocabulary;
  }

  this->vocabulary = new std::vector<std::string>;

  std::ifstream file;
  std::string line;
  file.open(filename);

  if (!file.is_open())
    std::__throw_runtime_error("Cannot open vocabulary file");

  while (std::getline(file, line))
  {
    if (line.size() > 0)
      this->vocabulary->push_back(line);
  }
}


void SemanticMap::save_best_matching_units_to_file(const std::string& filename) const
{
  assert (this->best_matching_units);

  std::cout << "Saving best matching units to '" << filename << "'" << std::endl;
  std::ofstream file;
  file.open(filename, std::ios::binary);

  if (!file.is_open())
    std::__throw_runtime_error("Cannot save best matching units");

  uint8_t _format = 0;
  assert (sizeof(*this->best_matching_units) == 2);

  write_uint8(file, _format);
  write_uint64(file, this->height);
  write_uint64(file, this->width);
  write_uint64(file, this->vocabulary_size);
  write_uint64(file, this->dataset_size);
  file.write((const char*) this->best_matching_units, this->dataset_size * sizeof(*this->best_matching_units));
  file.close();
}


void SemanticMap::load_counts_from_file(const std::string& filename)
{
  assert (!this->counts);

  std::cout << "  Loading count array from " << filename << std::endl;

  std::ifstream file;
  file.open(filename, std::ios::binary);

  if (!file.is_open())
    std::__throw_runtime_error("Unable to load counts from file");

  bool _big;
  uint8_t _format;
  uint64_t _height, _width, _vocabulary_size;

  file.read((char*)&_big, sizeof(_big));
  file.read((char*)&_format, sizeof(_format));
  if (_format != 0)
    std::__throw_runtime_error("Stored count array has unknown format");
  file.read((char*)&_height, sizeof(_height));
  file.read((char*)&_width, sizeof(_width));
  file.read((char*)&_vocabulary_size, sizeof(_vocabulary_size));

  this->height = static_cast<CellIndexType>(_height);
  this->width = static_cast<CellIndexType>(_width);
  this->vocabulary_size = static_cast<IndexType>(_vocabulary_size);

  this->num_cells = this->height * this->width;
  this->counts = new CountType[this->num_cells * this->vocabulary_size];
  try {
    file.read((char*) this->counts, this->num_cells * this->vocabulary_size * sizeof(*this->counts));
  } catch ( std::exception const & e ) {
    std::cerr << "Failed reading counts" << std::endl;
    if (this->counts)
      delete [] this->counts;
    this->counts = nullptr;
    file.close();
    throw e;
  }

  file.close();
}


void SemanticMap::load_best_matching_units_from_file(const std::string& filename)
{
  assert (!this->best_matching_units);

  std::cout << "  Loading best-matching-units from " << filename << std::endl;

  std::ifstream file;
  file.open(filename, std::ios::binary);

  if (!file.is_open())
    std::__throw_runtime_error("Unable to load best matching units from file");

  bool _big;
  uint8_t _format;
  uint64_t _height, _width, _vocabulary_size, _dataset_size;

  file.read((char*)&_big, sizeof(_big));
  file.read((char*)&_format, sizeof(_format));
  if (_format != 0)
    std::__throw_runtime_error("Stored BMU array has unknown format");
  file.read((char*)&_height, sizeof(_height));
  file.read((char*)&_width, sizeof(_width));
  file.read((char*)&_vocabulary_size, sizeof(_vocabulary_size));
  file.read((char*)&_dataset_size, sizeof(_dataset_size));

  this->height = static_cast<CellIndexType>(_height);
  this->width = static_cast<CellIndexType>(_width);
  this->vocabulary_size = static_cast<IndexType>(_vocabulary_size);
  this->dataset_size = static_cast<IndexPointerType>(_dataset_size);

  this->num_cells = this->height * this->width;
  this->best_matching_units = new CellIndexType[this->dataset_size];
  try {
    file.read((char*)this->best_matching_units, this->dataset_size * sizeof(*this->best_matching_units));
  } catch ( std::exception const & e ) {
    if (best_matching_units)
      delete [] this->best_matching_units;
    this->best_matching_units = nullptr;
    file.close();
    throw e;
  }

  file.close();
}


CountType SemanticMap::get_counts(const CellIndexType row, const CellIndexType col) const
{
  CellIndexType cell_index = row * this->width + col;
  CountType count = 0;
  for (IndexType vocab_index = 0; vocab_index < this->vocabulary_size; ++vocab_index)
  {
    count += this->counts[this->num_cells * vocab_index + cell_index];
  }
  return count;
}


CountType* SemanticMap::get_counts(const IndexType vocab_index) const
{
  return &this->counts[this->num_cells * vocab_index];
}
