
#define VERSION_MAJOR 3
#define VERSION_MINOR 4
#define VERSION_PATCH 0

#include <iostream>
#include <vector>
#include <iterator>
#include <algorithm>
#include <assert.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include "argparse.hpp"
#include "data.hpp"
#include "topo.hpp"
#include "som.hpp"
#include "smap.hpp"
#include "utils.hpp"


namespace fs = std::filesystem;


void create_semantic_map(ArgParser& args) {
  // Determine settings
  const std::string training_data_filename = args.get_option(1);
  const CellIndexType width = args.get_option_as_int(2);
  const CellIndexType height = args.get_option_as_int(3);
  const fs::path directory = args.get_option("--directory", "");
  const fs::path name = args.get_option("--name", "");
  const fs::path prior_name = args.get_option("--prior-name", "");
  const CellIndexType initial_radius = args.get_option_as_int("--initial-radius", (width + height) / 2);
  Float update_exponent = args.get_option_as_float("--update-exponent", 0.95);
  const unsigned int num_epochs = args.get_option_as_int("--epochs", 2);
  const auto global_topology = static_cast<GlobalTopology>(args.get_option_as_int("--global-topology", GlobalTopology::TORUS));
  const auto local_topology = static_cast<LocalTopology>(args.get_option_as_int("--local-topology", LocalTopology::CIRC));
  const bool verbose = args.option_exists("--verbose");
  const bool respect_lower_bound = !args.option_exists("--non-adaptive");
  const auto train_vocab_cutoff = static_cast<IndexType>(args.get_option_as_int("--train-vocab-cutoff", 0));  // If not zero, ignore all vocab indices above this when finding best matching units
  const auto dead_cell_update_strides = static_cast<IndexType>(args.get_option_as_int("--dead-cell-update-strides", 0));  // If not zero, assign dead cells to most distant inputs every nth epoch

  if (!args.option_exists("--update-exponent"))
    // Choose the update exponent such that the the minimal radius at the final epoch is 1.5
    update_exponent = std::pow(std::log(1.5), 1. / num_epochs) / std::pow(std::log(initial_radius), 1. / num_epochs);

  // Check settings
  if (name.empty())
    std::__throw_invalid_argument("Please provide a name with --name");
  if (directory.empty())
    std::__throw_invalid_argument("Please provide a base directory name with --name");
  if (num_epochs < 2)
    std::__throw_invalid_argument("The number of epochs must be at least 2");
  if (width < 1 || height < 1)
    std::__throw_invalid_argument("The map width or height must be at least 1");
  if (initial_radius < 1.f)
    std::__throw_invalid_argument("The initial radius must be at least 1");
  if (update_exponent <= 0.f || update_exponent > 1.f)
    std::__throw_invalid_argument("The update exponent must be a real number between 0 and 1");
  if (local_topology == LocalTopology::HEXA && (height&1) == 1)
    std::__throw_invalid_argument("For a hexagonal grid the number of rows has to be even");

  const fs::path codebook_save_filename = directory / name / fs::path("codebook.bin");
  const fs::path codebook_load_filename = prior_name.empty() ? "" : directory / prior_name / fs::path("codebook.bin");
  const fs::path best_matching_units_save_filename = directory / name / fs::path("bmus.bin");
  const fs::path neighbourhood_save_filename = directory / name / fs::path("neighbourhood.bin");
  const fs::path counts_save_filename = directory / name / fs::path("counts.bin");
  const fs::path convergence_log_filename = directory / name / fs::path("convergence.tsv");
  const fs::path readme_log_filename = directory / name / fs::path("README.md");
  const fs::path preliminary_output_directory = directory / name;

  if (!fs::exists(directory / name))
    fs::create_directory(directory / name);

  std::ofstream readme;
  readme.open(readme_log_filename.c_str(), std::ofstream::out);

  // Print settings
  std::cout << "Creating a semantic map '" << name << "' with " << std::endl
            << "Dimensions:            " << width << " x " << height << std::endl
            << "Initial update radius: " << initial_radius << std::endl
            << "Update exponent:       " << update_exponent << std::endl
            << "Respect lower bound:   " << respect_lower_bound << std::endl
            << "Local topology:        " << get_local_topology_string(local_topology)  << std::endl
            << "Global topology:       " << get_global_topology_string(global_topology) << std::endl
            << "Training vocab cutoff: " << train_vocab_cutoff << std::endl
            << "Number of epochs:      " << num_epochs << std::endl
            << "Dead cell updates:     " << dead_cell_update_strides << std::endl
            << std::endl;

  readme << "# Semantic Map " << name << std::endl
    << std::endl
    << "Semantic Map version:  " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << std::endl
    << "Verbose:               " << verbose << std::endl
    << "Prior map:             " << prior_name << std::endl
    << std::endl
    << "## Hyperparameters" << std::endl
    << "Dimensions:            " << width << " x " << height << std::endl
    << "Initial update radius: " << initial_radius << std::endl
    << "Update exponent:       " << update_exponent << std::endl
    << "Respect lower bound:   " << respect_lower_bound << std::endl
    << "Local topology:        " << get_local_topology_string(local_topology)  << std::endl
    << "Global topology:       " << get_global_topology_string(global_topology) << std::endl
    << "Training vocab cutoff: " << train_vocab_cutoff << std::endl
    << "Number of epochs:      " << num_epochs << std::endl
    << "Dead cell updates:     " << dead_cell_update_strides << std::endl
    << std::endl
    << "## Machine" << std::endl
    << "CPU:                   " << get_cpu_name() << std::endl
    << "Max. parallel threads: " << std::thread::hardware_concurrency() << std::endl
    << std::endl;

  std::ofstream convergence_log_stream;
  convergence_log_stream.open(convergence_log_filename.c_str(), std::ofstream::out);

  // Create semantic map
  auto stop_watch = StopWatch();
  stop_watch.start();
  auto* data = new CorpusDataset(training_data_filename);
  auto min_word_index_to_avoid_empty_row = data->min_word_index_to_avoid_empty_row();

  std::cout << "Number of snippets:     " << data->num_rows << std::endl
            << "Vocabulary size:        " << data->num_cols << std::endl
            << "Longest leading zeros:  " << min_word_index_to_avoid_empty_row << std::endl
            << "Total number of tokens: " << data->num_non_zero << std::endl;

  readme  << "## Dataset" << std::endl
          << "Number of snippets:     " << data->num_rows << std::endl
          << "Vocabulary size:        " << data->num_cols << std::endl
          << "Longest leading zeros:  " << min_word_index_to_avoid_empty_row << std::endl
          << "Total number of tokens: " << data->num_non_zero << std::endl
          << std::endl;

  if (train_vocab_cutoff > 0 && min_word_index_to_avoid_empty_row > train_vocab_cutoff)
    std::cout << "WARNING: Some training snippets are empty." << std::endl;

  if (train_vocab_cutoff > data->num_cols)
    std::__throw_invalid_argument("The vocabulary size is smaller than the training vocabulary cutoff.");

  data->init_sum_of_squares();

  Codebook* codebook;
  if (!codebook_load_filename.empty()) {
    std::cout << "Loading prior codebook from " << codebook_load_filename << std::endl;
    codebook = new Codebook(codebook_load_filename);
  } else {
    codebook = new Codebook(height, width, data->num_cols, global_topology, local_topology);
    codebook->init();
  }
  Neighbourhood* neighbourhood = new Neighbourhood(height, width, global_topology, local_topology, update_exponent, initial_radius);
  train(
    *codebook,
    *neighbourhood,
    *data,
    num_epochs,
    convergence_log_stream,
    verbose ? preliminary_output_directory.string() + "/" : "",
    respect_lower_bound,
    train_vocab_cutoff,
    dead_cell_update_strides
  );

  neighbourhood->save_to_file(neighbourhood_save_filename.string());
  delete neighbourhood;

  auto* semantic_map = new SemanticMap(*data, *codebook, train_vocab_cutoff);
  delete data;

  codebook->save_to_file(codebook_save_filename.string());
  delete codebook;

  semantic_map->save_best_matching_units_to_file(best_matching_units_save_filename.string());
  delete semantic_map;

  stop_watch.stop();
  std::cout << "Creating the semantic map took " << stop_watch << std::endl;

  readme << "## Timing" << std::endl
         << "Creation started at UnixTime:   " << stop_watch.get_start_unix_time() << std::endl
         << "Creation ended at UnixTime:     " << get_unix_time() << std::endl
         << "Creating the semantic map took: " << stop_watch << std::endl;

  readme.close();
  convergence_log_stream.close();
}


int main(int argc, char* argv[])
{
  if (is_big_endian())
  {
    // ToDo: Implement import/export routines for big endian systems
    std::cerr << "Sorry, smap does not work on big endian systems" << std::endl;
    return 1;
  }

  try
  {
    ArgParser args(argc, argv);
    std::string mode = args.get_option(0);
    if (mode == "create") {
      create_semantic_map(args);
    } else if (mode == "--author") {
      std::cout << "Created by Johannes E. M. Mosig (j.mosig@rasa.com)" << std::endl;
    } else if (mode == "--version") {
      std::cout << "v" << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << std::endl;
    } else if (mode == "--help" || mode == "-h") {
      std::cout << "Maximum vocabulary size: " << MAX_INDEX_SIZE << std::endl;
    } else {
      std::__throw_invalid_argument("Unknown mode");
    }

  } catch (const std::exception &exc) {
      std::cerr << exc.what() << std::endl;
  }

  return 0;
}
