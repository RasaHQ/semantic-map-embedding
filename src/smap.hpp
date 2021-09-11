
#pragma once

#include <string>
#include "som.hpp"
#include "data.hpp"


class SemanticMap
{
public:
  SemanticMap();
  SemanticMap(const std::string& counts_filename);
  SemanticMap(const std::string& counts_filename, const std::string& best_matching_units_filename);
  SemanticMap(const CorpusDataset& data, const Codebook& codebook, const IndexType train_vocab_cutoff);
  ~SemanticMap();

  void delete_counts();

  void build(const CorpusDataset& data, const Codebook& codebook, IndexType train_vocab_cutoff);
  void build(const CorpusDataset& data, CellIndexType* best_matching_units, const CellIndexType _height, const CellIndexType _width);
  std::vector<size_t> find_snippets(const CorpusDataset& data, CellIndexType map_row, CellIndexType map_col);
  
  void associate_vocabulary(const std::string& filename);
  
  void save_counts_to_file(const std::string& filename) const;
  void save_best_matching_units_to_file(const std::string& filename) const;

  CountType get_counts(const CellIndexType row, const CellIndexType col) const;
  CountType* get_counts(const IndexType vocab_index) const;

protected:
  void load_counts_from_file(const std::string& filename);
  void load_best_matching_units_from_file(const std::string& filename);

  void build_counts(const CorpusDataset& data);

  CountType* counts;
  CellIndexType* best_matching_units;
  std::vector<std::string>* vocabulary;
  IndexType vocabulary_size;
  IndexPointerType dataset_size;
  CellIndexType height;
  CellIndexType width;
  CellIndexType num_cells;

  bool should_cleanup_best_matching_units;
};


IndexType* term_counts(const CorpusDataset& data, std::vector<size_t>& associated_snippets);
