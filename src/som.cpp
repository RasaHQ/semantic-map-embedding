
#include <iostream>
#include <fstream>
#include <iterator>
#include <string>
#include <sstream>
#include <random>
#include <assert.h>
#include <algorithm>  // fill_n

#if defined(_OPENMP)
  #include <omp.h>
#endif

#include "som.hpp"
#include "topo.hpp"
#include "utils.hpp"
#include "smap.hpp"


#define SQRT_E 1.6487212707001281468486507878142


Neighbourhood::Neighbourhood(
    const CellIndexType height, 
    const CellIndexType width, 
    const GlobalTopology golbal_topology, 
    const LocalTopology local_topology, 
    const Float update_exponent,
    const CellIndexType initial_radius
  ) :
  height(height),
  width(width),
  distance(distance_function(golbal_topology, local_topology)),
  update_exponent(update_exponent),
  initial_radius(initial_radius),
  radius_min(initial_radius),
  radius_max(initial_radius),
  values(nullptr)
{
  this->num_cells = this->height * this->width;
  this->values = new Float[this->num_cells];
  // ToDo: Randomize initial radii?
  std::fill_n(this->values, this->num_cells, static_cast<Float>(initial_radius));
}


Neighbourhood::~Neighbourhood()
{
  if (this->values)
  {
    delete [] this->values;
    this->values = nullptr;
  }
}


Float Neighbourhood::influence(const CellIndexType source_cell, const CellIndexType target_cell) const
{
  assert (this->values);
  CellIndexType y1 = source_cell / this->width,
                x1 = source_cell % this->width,
                y2 = target_cell / this->width,
                x2 = target_cell % this->width;
  const CellIndexType d = this->distance(y1, x1, y2, x2, this->width, this->height);
  const Float r = this->values[target_cell];
  // Based on Equation (3) of Kiviluoto (DOI 10.1109/ICNN.1996.548907)
  return d < r ? (1. - SQRT_E * std::exp(-0.5 * squared(d) / squared(r))) / (r * (1. - SQRT_E)) : 0.f;
}


Float Neighbourhood::update(
  const CellIndexType* const best_matching_units, 
  const CellIndexType* const next_best_matching_units, 
  const IndexPointerType num_rows,
  const bool respect_lower_bound
)
{
  const auto discontinuities = this->topographic_discontinuities(best_matching_units, next_best_matching_units, num_rows);
  this->radius_min = MAX_REAL_DISTANCE;
  this->radius_max = 0.f;

  #pragma omp parallel for
  for (CellIndexType cell_index = 0; cell_index < this->num_cells; ++cell_index) 
  {
    Float radius_lower_bound = 1.f;
    for (auto const& discontinuity : std::as_const(discontinuities))
    {
      radius_lower_bound = std::max(
        radius_lower_bound, 
        static_cast<Float>(this->radius_from_discontinuity(cell_index, discontinuity))
      );
    }

    if (respect_lower_bound)
    {
      this->values[cell_index] = std::max(
        radius_lower_bound,
        std::pow(this->values[cell_index], this->update_exponent)
      );
    }
    else
    {
      this->values[cell_index] = std::pow(this->values[cell_index], this->update_exponent);
    }

    this->radius_min = std::min(this->radius_min, this->values[cell_index]);
    this->radius_max = std::max(this->radius_max, this->values[cell_index]);
    
  }

  // Return the topographic error
  return static_cast<Float>(discontinuities.size() + 1) / num_rows;
}


CellIndexType Neighbourhood::radius_from_discontinuity(CellIndexType cell_index, TopographicDiscontinuity discontinuity) const 
{
  // Based on Equation (5) of Kiviluoto (DOI 10.1109/ICNN.1996.548907)
  CellIndexType y = cell_index / this->width,
                x = cell_index % this->width,
                y1 = discontinuity.cell1 / this->width,
                x1 = discontinuity.cell1 % this->width,
                y2 = discontinuity.cell2 / this->width,
                x2 = discontinuity.cell2 % this->width;
  const CellIndexType d1 = this->distance(y, x, y1, x1, this->height, this->width);
  const CellIndexType d2 = this->distance(y, x, y2, x2, this->height, this->width);
  if (std::max(d1, d2) <= discontinuity.distance)
  {
    return discontinuity.distance;
  }
  else if (std::min(d1, d2) < discontinuity.distance)
  {
    assert (discontinuity.distance - std::min(d1, d2) >= 1);
    return discontinuity.distance - std::min(d1, d2);
  }
  else
  {
    return 1;
  }  
}


std::vector<TopographicDiscontinuity> Neighbourhood::topographic_discontinuities(
  const CellIndexType* best_matching_units,
  const CellIndexType* next_best_matching_units,
  const IndexPointerType num_rows
) const
{
  CellIndexType distance;
  CellIndexType y1, x1, y2, x2;

  std::vector<TopographicDiscontinuity> discontinuities;

  CellIndexType cell1, cell2;
  for (IndexPointerType row = 0; row < num_rows; ++row)
  {
    cell1 = best_matching_units[row];
    cell2 = next_best_matching_units[row];
    y1 = cell1 / this->width;
    x1 = cell1 % this->width;
    y2 = cell2 / this->width;
    x2 = cell2 % this->width;
    distance = this->distance(y1, x1, y2, x2, this->height, this->width);
    if (distance > 1)
    {
      discontinuities.push_back(
        TopographicDiscontinuity(cell1, cell2, distance)
      );
    }
  }
  return discontinuities;
}


void Neighbourhood::save_to_file(const std::string& filename) const
{
  std::cout << "Saving neighbourhood to '" << filename << "'" << std::endl;
  std::ofstream file;
  file.open(filename, std::ios::binary);

  if (!file.is_open())
    std::__throw_runtime_error("Unable to save neighbourhood to file");

  const uint8_t format = 0;
  assert (sizeof(*this->values) == 4);

  write_uint8(file, format);
  write_uint64(file, this->height);
  write_uint64(file, this->width);
  file.write((const char*) this->values, this->height * this->width * sizeof(*this->values));

  file.close();
}


Codebook::Codebook(
    CellIndexType height, 
    CellIndexType width, 
    IndexType input_dim,
    GlobalTopology global_topology,
    LocalTopology local_topology
  ) : 
    width(width),
    height(height),
    input_dim(input_dim),
    global_topology(global_topology),
    local_topology(local_topology)
{
  this->num_cells = height * width;
  this->size = this->num_cells * input_dim;
  size_t required_bytes = this->size * sizeof(Float);

  this->distance = distance_function(global_topology, local_topology);

  try {
    this->array.reserve(this->size);
  } catch (std::bad_alloc& e) {
    std::cerr << "Failed to allocate " << required_bytes << " bytes of memory for codebook";
    throw e;
  }
}


Codebook::Codebook(
    const std::string& filename
  ) : 
    width(0),
    height(0),
    input_dim(0),
    num_cells(0),
    size(0),
    global_topology(GlobalTopology::PLANE),
    local_topology(LocalTopology::CIRC)
{
  this->load_from_file(filename);
}


Codebook::~Codebook()
{}


void Codebook::init(int _seed, bool _increment_seed_by_thread_number)
{
  std::cout << "Initializing codebook" << std::endl;
  int seed = _seed;
  this->array.resize(this->size);

  #pragma omp parallel firstprivate(seed)
  {
    #if defined(_OPENMP)
    // Set a unique seed for each thread
    if (_increment_seed_by_thread_number)
      seed += omp_get_thread_num();
    #endif

    std::default_random_engine random_number_generator(seed);
    std::uniform_real_distribution<Float> uniform(0.f, 1.f);

    #pragma omp for
    for (IndexPointerType i = 0; i < this->size; i++)
    {
        this->array[i] = uniform(random_number_generator);
    }
  }
}


void Codebook::init()
{
  this->init(static_cast<int>(get_unix_time()), true);
}


void Codebook::save_to_file(const std::string& filename) const
{
  std::cout << "Saving codebook to '" << filename << "'" << std::endl;
  std::ofstream file;
  file.open(filename, std::ios::binary);

  if (!file.is_open())
    std::__throw_runtime_error("Unable to save codebook to file");

  uint8_t format = 0;

  write_uint8(file, format);
  write_uint64(file, this->height);
  write_uint64(file, this->width);
  write_uint64(file, this->input_dim);
  file.write(reinterpret_cast<const char*>(this->array.data()), this->size * sizeof(Float));

  file.close();
}


void Codebook::load_from_file(const std::string& filename)
{
  std::ifstream file;
  file.open(filename, std::ios::binary);
  file.unsetf(std::ios::skipws);  // See https://stackoverflow.com/a/21802936/6760298

  if (!file.is_open())
    std::__throw_runtime_error("Unable to load codebook from file");

  uint8_t format = read_uint8(file);
  if (format != 0)
    std::__throw_runtime_error("Stored codebook has unknown format");
  this->height = static_cast<CellIndexType>(read_uint64(file));
  this->width = static_cast<CellIndexType>(read_uint64(file));
  this->input_dim = static_cast<IndexType>(read_uint64(file));
  this->num_cells = this->height * this->width;
  this->size = this->num_cells * this->input_dim;
  this->array.clear();
  size_t required_bytes = this->size * sizeof(Float);
  try {
    this->array.reserve(this->size);
    this->array.resize(this->size);
  } catch (std::bad_alloc& e) {
    std::cerr << "Failed to allocate " << required_bytes << " bytes of memory for codebook";
    throw e;
  }

  try {
    file.read((char*) this->array.data(), this->size * sizeof(Float));
  } catch ( std::exception const & e ) {
    this->array.clear();
    file.close();
    throw e;
  }

  file.close();
}


Float Codebook::get_value(IndexPointerType index)
{
  if (index < this->size) {
    return this->array[index];
  } else {
    std::__throw_length_error("Codebook has no entry with given index");
  }
}


// static Float product_with_weights(const IndexType* const indices, const IndexType num_non_zero, const Float* const values, const WeightType* const weights)
// {
//     Float result = 0.;

//     for (size_t it = 0; it < num_non_zero; ++it)
//     {
//       const IndexType idx = indices[it];
//       result += values[idx] * weights[idx];
//     }
//     return result;
// }


static Float product_with_weights(const IndexType* const indices, const IndexType num_non_zero, const Float* const values, const WeightType* const weights, const IndexType effective_input_dim)
{
    Float result = 0.;

    for (size_t it = 0; it < num_non_zero; ++it)
    {
      const IndexType idx = indices[it];
      if (idx < effective_input_dim)
      {
        result += values[idx] * weights[idx];
      } 
      else 
      {
        break;  // Indices are sorted, so all further indices will be even larger
      }
    }
    return result;
}


static Float product(const IndexType* const indices, const IndexType num_non_zero, const Float* const values, const IndexType effective_input_dim)
{
    Float result = 0.;

    for (size_t it = 0; it < num_non_zero; ++it)
    {
      const IndexType idx = indices[it];

      if (idx >= effective_input_dim)
        break;  // Indices are sorted, so all further indices will be even larger

      result += values[idx];
    }
    return result;
}


// static Float product(const IndexType* const indices, const IndexType num_non_zero, const Float* const values)
// {
//     Float result = 0.;

//     for (size_t it = 0; it < num_non_zero; ++it)
//     {
//       const IndexType idx = indices[it];
//       result += values[idx];
//     }
//     return result;
// }


void Codebook::find_best_matching_units(
    const BinarySparseMatrix& data, 
    CellIndexType* const best_matching_units, 
    Float* const distances, 
    IndexType train_vocab_cutoff,
    bool need_correct_distances
  ) const 
{
  assert (data._sum_of_squares || !need_correct_distances);

  const auto effective_input_dim = (train_vocab_cutoff > 0 ? train_vocab_cutoff : data.num_cols);

  std::fill_n(best_matching_units, data.num_rows, 0);
  std::fill_n(distances, data.num_rows, MAX_REAL_DISTANCE);
  
  for (size_t cell_index = 0; cell_index < this->num_cells; ++cell_index)
  {
    // See https://stackoverflow.com/a/34488841
    const Float* const w = &this->array[cell_index * this->input_dim];

    const Float w_squared = vec_squared(w, this->input_dim);

    #pragma omp parallel for
    for (IndexPointerType row = 0; row < data.num_rows; ++row)
    {
      const IndexType* const x = data.indices_in_row(row);     // Address of the beginning of the array of columns-with-value-indices
      const WeightType* const weights = data.weights_in_row(row);     // Address of the beginning of the array of weight-indices
      const IndexType num_non_zero_in_row = data.num_indices_in_row(row);

      if (num_non_zero_in_row == 0)
        continue;

      if (x[0] >= effective_input_dim)
        continue;

      Float distance;
      if (data.has_weights())
      {
        distance = w_squared - 2 * product_with_weights(x, num_non_zero_in_row, w, weights, effective_input_dim);
      } else {
        distance = w_squared - 2 * product(x, num_non_zero_in_row, w, effective_input_dim);
      }

      if (distance < distances[row])
      {
        best_matching_units[row] = cell_index;
        distances[row] = distance;
      }
    }
  }

  if (need_correct_distances)
  {
    #pragma omp parallel for
    for (IndexType row = 0; row < static_cast<IndexType>(data.num_rows); ++row)
    {
      distances[row] = std::max(0.f, distances[row] + data._sum_of_squares[row]);
    }
  }
}


void Codebook::find_best_and_next_best_matching_units(
    const BinarySparseMatrix& data, 
    CellIndexType* const best_matching_units, 
    Float* const distances, 
    CellIndexType* const next_best_matching_units, 
    Float* const next_distances,
    const IndexType train_vocab_cutoff
  ) const 
{
  assert (data._sum_of_squares);

  std::fill_n(best_matching_units, data.num_rows, 0);
  std::fill_n(next_best_matching_units, data.num_rows, 0);
  std::fill_n(distances, data.num_rows, MAX_REAL_DISTANCE);
  std::fill_n(next_distances, data.num_rows, MAX_REAL_DISTANCE);

  const auto effective_input_dim = (train_vocab_cutoff > 0 ? train_vocab_cutoff : this->input_dim);
  
  for (CellIndexType cell_index = 0; cell_index < this->num_cells; ++cell_index)
  {
    // See https://stackoverflow.com/a/34488841
    const Float* const w = &this->array[cell_index * effective_input_dim];

    const Float w_squared = vec_squared(w, effective_input_dim);

    #pragma omp parallel for
    for (IndexPointerType row = 0; row < data.num_rows; ++row)
    {
      const IndexType* const indices = data.indices_in_row(row);     // Address of the beginning of the array of columns-with-value-indices
      const WeightType* const weights = data.weights_in_row(row);     // Address of the beginning of the array of weight-indices
      const IndexType num_non_zero_in_row = data.num_indices_in_row(row);

      if (num_non_zero_in_row == 0)
        continue;

      if (indices[0] >= effective_input_dim)
        continue;

      Float distance;
      if (data.has_weights())
      {
        // We consider the weights for finding the best matching units here, 
        // because we want the dimensions with weight > 1 to be more important.
        // Thus, they contribute more to the distance.
        // We do not use weights for updating in `apply_batch_som_update`, so all
        // inputs and codebook vectors are in [0, 1].
        distance = w_squared - 2 * product_with_weights(indices, num_non_zero_in_row, w, weights, effective_input_dim) + data._sum_of_squares[row];
      } else {
        distance = w_squared - 2 * product(indices, num_non_zero_in_row, w, effective_input_dim) + data._sum_of_squares[row];
      }

      if (distance < distances[row])
      {
        next_best_matching_units[row] = best_matching_units[row];
        next_distances[row] = distances[row];
        best_matching_units[row] = cell_index;
        distances[row] = std::max(0.f, distance);
      }
    }
  }
}


void Codebook::apply_batch_som_update(
  const BinarySparseMatrix& data, 
  const Neighbourhood& neighbourhood,
  const CellIndexType* const best_matching_units,
  const IndexType train_vocab_cutoff
)
{
  #pragma omp parallel
  {
    const auto effective_input_dim = (train_vocab_cutoff > 0 ? train_vocab_cutoff : this->input_dim);
    auto* const numerator = new Float[this->input_dim];

    #pragma omp for
    for (CellIndexType cell_index = 0; cell_index < this->num_cells; ++cell_index) 
    {      
      Float denominator = 0.f;
      std::fill_n(numerator, this->input_dim, 0.f);

      for (IndexPointerType row = 0; row < data.num_rows; ++row)
      {
        const IndexType* indices = data.indices_in_row(row);
        const IndexType num_non_zero_in_row = data.num_indices_in_row(row);

        const Float learning_rate = neighbourhood.influence(best_matching_units[row], cell_index);

        if (learning_rate <= 0.)
          continue;

        denominator += learning_rate;
        for (size_t i = 0; i < num_non_zero_in_row; ++i)
        {
          if (indices[i] >= effective_input_dim)
            break;
          numerator[indices[i]] += learning_rate;  // * 1.0 (input data is binary)
        }
      }

      if (denominator != 0)
      {
        Float * const w = &this->array[cell_index * this->input_dim];
        for (IndexType i = 0; i < this->input_dim; ++i)
        {
          w[i] = numerator[i] / denominator;
        }
      }
    }
    delete [] numerator;
  }
}


Float Codebook::quantization_error(
  Float* const distances, 
  IndexPointerType const num_rows
) const
{
  Float error = 0;
  for (IndexPointerType row = 0; row < num_rows; ++row)
  {
    assert (distances[row] >= 0.f);
    error += squared(distances[row]);
  }
  error = std::sqrt(error) / num_rows;
  return error;
}


Float Codebook::gap_error(CellIndexType* const best_matching_units, const IndexPointerType num_rows) const
{
  auto* cell_in_use = new bool[this->num_cells];
  std::fill_n(cell_in_use, this->num_cells, false);
  CellIndexType num_cells_used = 0;
  for (IndexPointerType row = 0; row < num_rows && num_cells_used < this->num_cells; ++row)
  {
    if (!cell_in_use[best_matching_units[row]])
    {
      cell_in_use[best_matching_units[row]] = true;
      num_cells_used += 1;
    }
  }
  delete [] cell_in_use;
  return static_cast<Float>(this->num_cells - num_cells_used) / this->num_cells;
}


Float Codebook::assign_dead_cells(CellIndexType* best_matching_units, Float* const distances, const IndexPointerType num_rows) const
{
  auto* cell_in_use = new bool[this->num_cells];
  std::fill_n(cell_in_use, this->num_cells, false);
  CellIndexType num_cells_used = 0;

  // Find unused cells
  for (IndexPointerType row = 0; row < num_rows && num_cells_used < this->num_cells; ++row)
  {
    if (!cell_in_use[best_matching_units[row]])
    {
      cell_in_use[best_matching_units[row]] = true;
      num_cells_used += 1;
    }
  }

  const CellIndexType num_cells_unused = this->num_cells - num_cells_used;
  if (num_cells_unused == 0 || num_cells_unused > num_rows)
  {
    delete [] cell_in_use;
    return 0.f;
  }

  std::cout << "    Found " << num_cells_unused << " dead units" << std::endl;

  // Find worst matching inputs (those with the largest distances)
  auto* largest_distances = new Float[num_cells_unused];
  std::partial_sort_copy(
    distances, distances + num_rows, 
    largest_distances, largest_distances + num_cells_unused, 
    std::greater<Float>()
  );
  const Float distance_threshold = largest_distances[num_cells_unused - 1];
  delete [] largest_distances;
  std::cout << "    Distance threshold: " << distance_threshold << std::endl;

  auto* worst_matching_inputs = new IndexPointerType[num_cells_unused];
  CellIndexType i = 0;
  for (IndexPointerType row = 0; row < num_rows && i < num_cells_unused; ++row)
  {
    if (distances[row] >= distance_threshold)
    {
      worst_matching_inputs[i++] = row;
    }
  }

  // Assign unused cells to one of the worst matching inputs
  i = 0;
  for (CellIndexType cell = 0; cell < this->num_cells && i < num_cells_unused; ++cell)
  {
    if (!cell_in_use[cell])
    {
      const IndexPointerType row = worst_matching_inputs[i++];
      best_matching_units[row] = cell;
    }
  }

  delete [] worst_matching_inputs;
  delete [] cell_in_use;

  // Return gap error
  return static_cast<Float>(num_cells_unused) / this->num_cells;
}


Float Codebook::diffusion_error(
    const CellIndexType* const best_matching_units,
    const CellIndexType* const previous_best_matching_units,
    const IndexPointerType num_rows
  ) const
{
  IndexPointerType total_distance = 0;
  for (IndexPointerType row = 0; row < num_rows; ++row)
  {
    const CellIndexType source_cell = previous_best_matching_units[row];
    const CellIndexType target_cell = best_matching_units[row];
    if (source_cell != target_cell)
    {
      CellIndexType y1 = source_cell / this->width,
                x1 = source_cell % this->width,
                y2 = target_cell / this->width,
                x2 = target_cell % this->width;
      total_distance += this->distance(y1, x1, y2, x2, this->width, this->height);
    }
  }
  return static_cast<Float>(total_distance) / static_cast<Float>(num_rows);
}


TopographicDiscontinuity::TopographicDiscontinuity(CellIndexType cell1, CellIndexType cell2, CellIndexType distance) :
  cell1(cell1),
  cell2(cell2),
  distance(distance)
{}


void train(
  Codebook& codebook,
  Neighbourhood& neighbourhood,
  CorpusDataset& data,
  const unsigned int num_epochs,
  std::ofstream& convergence_log_stream,
  const std::string& directory,
  const bool respect_lower_bound,
  const IndexType train_vocab_cutoff,
  const unsigned int dead_cell_update_strides
)
{
  std::cout << "Training adaptive self-organizing map" << std::endl;
  assert (num_epochs > 1);
  assert (convergence_log_stream.is_open());

  auto* best_matching_units = new CellIndexType[data.num_rows];
  auto* previous_best_matching_units = new CellIndexType[data.num_rows];
  auto* distances = new Float[data.num_rows];
  auto* next_best_matching_units = new CellIndexType[data.num_rows];
  auto* next_distances = new Float[data.num_rows];
  Float diffusion_error = 0.f;
  Float gap_error = 0.f;

  convergence_log_stream << "Epoch\tUnixTime\tRadiusMin\tRadiusMax\tQuantizationError\tTopographicError\tGapError\tDiffusionError" << std::endl;

  for (unsigned int epoch = 1; epoch <= num_epochs; ++epoch)
  {
    std::cout << "Epoch " << epoch << " of " << num_epochs << std::endl;

    std::cout << "  Find best matching units" << std::endl;
    codebook.find_best_and_next_best_matching_units(data, best_matching_units, distances, next_best_matching_units, next_distances, train_vocab_cutoff);
    if (dead_cell_update_strides > 0 && epoch % dead_cell_update_strides == 0)
    {
      std::cout << "  Assign dead units" << std::endl;
      gap_error = codebook.assign_dead_cells(best_matching_units, distances, data.num_rows);
    } else {
      gap_error = codebook.gap_error(best_matching_units, data.num_rows);
    }

    if (epoch > 1)
    {
      diffusion_error = codebook.diffusion_error(best_matching_units, previous_best_matching_units, data.num_rows);
    }
    std::copy(best_matching_units, best_matching_units + data.num_rows, previous_best_matching_units);

    if (directory.length() > 0)
    {
      std::ofstream file;
      std::stringstream preliminary_r_filename;
      preliminary_r_filename << directory << "prelim-" << epoch - 1 << ".neighbourhood.bin";
      neighbourhood.save_to_file(preliminary_r_filename.str());
    }

    std::cout << "  Apply batch-som update" << std::endl;
    if (epoch < num_epochs)
    {
      codebook.apply_batch_som_update(data, neighbourhood, best_matching_units, train_vocab_cutoff);
    } else {
      codebook.apply_batch_som_update(data, neighbourhood, best_matching_units);
    }

    std::cout << "  Update neighbourhoods" << std::endl;
    Float topographic_error = neighbourhood.update(best_matching_units, next_best_matching_units, data.num_rows, respect_lower_bound);

    // Log the current error metrics      
    convergence_log_stream << epoch - 1 
      << "\t" << get_unix_time() 
      << "\t" << neighbourhood.get_radius_min()
      << "\t" << neighbourhood.get_radius_max()
      << "\t" << codebook.quantization_error(distances, data.num_rows) 
      << "\t" << topographic_error 
      << "\t" << gap_error
      << "\t" << diffusion_error
      << std::endl;
  }

  // Log the final error metrics
  codebook.find_best_and_next_best_matching_units(data, best_matching_units, distances, next_best_matching_units, next_distances, train_vocab_cutoff);
  gap_error = codebook.gap_error(best_matching_units,data.num_rows);
  Float topographic_error = neighbourhood.update(best_matching_units, next_best_matching_units, data.num_rows, respect_lower_bound);
  diffusion_error = codebook.diffusion_error(best_matching_units, previous_best_matching_units, data.num_rows);
  convergence_log_stream << num_epochs 
      << "\t" << get_unix_time() 
      << "\t" << neighbourhood.get_radius_min()
      << "\t" << neighbourhood.get_radius_max()
      << "\t" << codebook.quantization_error(distances, data.num_rows) 
      << "\t" << topographic_error 
      << "\t" << gap_error
      << "\t" << diffusion_error
      << std::endl;
  
  delete [] best_matching_units;
  delete [] distances;
  delete [] next_best_matching_units;
  delete [] next_distances;
}
