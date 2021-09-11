
#pragma once

#include <string>
#include "data.hpp"
#include "topo.hpp"


struct TopographicDiscontinuity
{
  TopographicDiscontinuity(CellIndexType cell1, CellIndexType cell2, CellIndexType distance);
  CellIndexType cell1, cell2;
  CellIndexType distance;
};


class Neighbourhood
{
public:
  Neighbourhood(
    const CellIndexType height, 
    const CellIndexType width, 
    const GlobalTopology golbal_topology, 
    const LocalTopology local_topology, 
    const Float update_exponent,
    const CellIndexType initial_radius
  );
  ~Neighbourhood();

  Float update(
    const CellIndexType* const best_matching_units, 
    const CellIndexType* const next_best_matching_units, 
    const IndexPointerType num_rows,
    const bool respect_lower_bound = true
  );
  void save_to_file(const std::string& filename) const;
  Float influence(const CellIndexType source_cell, const CellIndexType target_cell) const;
  inline Float get_radius_min() { return this->radius_min; }
  inline Float get_radius_max() { return this->radius_max; }

private:
  std::vector<TopographicDiscontinuity> topographic_discontinuities(
    const CellIndexType* best_matching_units,
    const CellIndexType* next_best_matching_units,
    const IndexPointerType num_rows
  ) const;
  CellIndexType radius_from_discontinuity(
    CellIndexType cell_index, 
    TopographicDiscontinuity discontinuity
  ) const;

private:
  CellIndexType height, width, num_cells;
  DistanceFunction distance;
  Float update_exponent;
  CellIndexType initial_radius;
  Float radius_min, radius_max;
  Float* values;
};


class Codebook
{
public:
  Codebook(const std::string& filename);
  Codebook(
    CellIndexType height, 
    CellIndexType width, 
    IndexType input_dim,
    GlobalTopology global_topology,
    LocalTopology local_topology
  );
  ~Codebook();

public:
  void init();
  void init(int _seed, bool _increment_seed_by_thread_number);

  void save_to_file(const std::string& filename) const;

  void find_best_matching_units(
    const BinarySparseMatrix& data, 
    CellIndexType* const best_matching_units, 
    Float* const distances, 
    IndexType train_vocab_cutoff, 
    bool need_correct_distances=true
  ) const;  
  
  void find_best_and_next_best_matching_units(
    const BinarySparseMatrix& data, 
    CellIndexType* const best_matching_units, 
    Float* const distances, 
    CellIndexType* const next_best_matching_units, 
    Float* const next_distances,
    const IndexType train_vocab_cutoff
  ) const;

  void apply_batch_som_update(
    const BinarySparseMatrix& data, 
    const Neighbourhood& neighbourhood,
    const CellIndexType* const best_matching_units,
    const IndexType train_vocab_cutoff = 0
  );

  Float quantization_error(
    Float* const distances, 
    IndexPointerType const num_rows
  ) const;

  Float gap_error(
    CellIndexType* const best_matching_units, 
    const IndexPointerType num_rows
  ) const;

  Float diffusion_error(
    const CellIndexType* const previous_best_matching_units,
    const CellIndexType* const best_matching_units,
    const IndexPointerType num_rows
  ) const;

  Float assign_dead_cells(
    CellIndexType* const best_matching_units, 
    Float* const distances,
    const IndexPointerType num_rows
  ) const;

  Float get_value(IndexPointerType index);

  inline IndexType get_input_dim() const {
    return this->input_dim;
  }

  inline CellIndexType get_num_cells() const {
    return this->num_cells;
  }

  inline CellIndexType get_height() const {
    return this->height;
  }
  
  inline CellIndexType get_width() const {
    return this->width;
  }

protected:
  void load_from_file(const std::string& filename);

  CellIndexType width;
  CellIndexType height;
  IndexType input_dim;

  CellIndexType num_cells;
  IndexPointerType size;

  GlobalTopology global_topology;
  LocalTopology local_topology;

  DistanceFunction distance;
  
  std::vector<Float> array;
};


void train(
  Codebook& codebook,
  Neighbourhood& neighbourhood,
  CorpusDataset& data,
  const unsigned int num_epochs,
  std::ofstream& convergence_log_stream,
  const std::string& directory_name = "",
  const bool respect_lower_bound = true,
  const IndexType train_vocab_cutoff = 0,
  const unsigned int dead_cell_update_strides = 0
);
