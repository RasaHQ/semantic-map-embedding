
#pragma once

#include <vector>
#include <string>
#include <limits>
#include <fstream>
#include <sys/stat.h>

typedef uint32_t IndexType;					// Can enumerate the vocabulary
typedef uint32_t IndexPointerType;	// Can enumerate all training data
typedef uint16_t CellIndexType;			// Can enumerate all cells in the map (e.g. 128**2)
typedef uint32_t CountType;					// Can represent the largest number of snippets that contain a specific term and are associated with a cell
typedef uint8_t WeightType;					// Can enumerate all weight classes (document title to text body)
typedef _Float32 Float;							// Regular precision floats
typedef _Float64 Double;						// High precision floats

const Float MAX_REAL_DISTANCE = std::numeric_limits<Float>::max();
const Float MAX_INTEGER_DISTANCE = std::numeric_limits<CellIndexType>::max();
const CountType MAX_COUNT = std::numeric_limits<CountType>::max();
const IndexType MAX_INDEX_SIZE = std::numeric_limits<IndexType>::max();
const IndexPointerType MAX_INDEX_POINTER_SIZE = std::numeric_limits<IndexPointerType>::max();


inline bool file_exists (const std::string& filename) 
{
  // Returns `true` iff the given file exists
  // https://stackoverflow.com/a/12774387/6760298
  struct stat buffer;   
  return (stat (filename.c_str(), &buffer) == 0); 
}


inline bool is_big_endian()
{
    union {
        uint32_t i;
        char c[4];
    } temp = {0x01020304};

    return temp.c[0] == 1; 
}


template<typename T> inline void write_uint64(std::ofstream& file, const T value)
{
	uint64_t _value = static_cast<uint64_t>(value);
	file.write((const char*) &_value, sizeof(uint64_t));
}


inline uint8_t read_uint8(std::ifstream& file)
{
	uint8_t _value;
	file.read((char*)&_value, sizeof(_value));
	return _value;
}


inline uint64_t read_uint64(std::ifstream& file)
{
	uint64_t _value;
	file.read((char*)&_value, sizeof(_value));
	return _value;
}


template<typename T> inline void write_uint8(std::ofstream& file, const T value)
{
	uint8_t _value = static_cast<uint8_t>(value);
	file.write((const char*) &_value, sizeof(uint8_t));
}


class BinarySparseMatrix
{
public:
	~BinarySparseMatrix()
	{
		if (this->_sum_of_squares) 
			delete [] this->_sum_of_squares;
	}

	void init_sum_of_squares();
	IndexType min_word_index_to_avoid_empty_row();

	std::vector<IndexType> indices;
	std::vector<IndexPointerType> index_pointers;
	std::vector<WeightType> weights;
	IndexPointerType num_rows;
	IndexPointerType num_text_rows;
	IndexType num_cols;
	IndexPointerType num_non_zero;
	IndexType* _sum_of_squares;

	// Address of the beginning of the array of columns-with-value>0-indices
	// Note: Using `inline` here and for `num_indices_in_row` improves performance by
	//       reducing computation time in one example from 65s to 61s
	inline const IndexType* indices_in_row(size_t row) const
	{
		// Index of where the values of this row of data start
		const IndexPointerType index_pointer = this->index_pointers[row];
		return &this->indices[index_pointer];
	}

	// Address of the beginning of the array of weights
	inline const WeightType* weights_in_row(size_t row) const
	{
		// Index of where the values of this row of data start
		const IndexPointerType index_pointer = this->index_pointers[row];
		return &this->weights[index_pointer];
	}

	// Number of columns-with-value-1-indices in given row
	inline IndexType num_indices_in_row(size_t row) const
	{
		return this->index_pointers[row + 1] - this->index_pointers[row];
	}

	// 
	// inline IndexType lowest_index(size_t row) const
	// {
	// 	return 
	// }

	inline bool has_weights() const
	{
		return this->_has_weights;
	}

protected:
	bool _has_weights;
};


class CorpusDataset : public BinarySparseMatrix
{
public:
	CorpusDataset(const std::string& filename);                   // Load from filename
	CorpusDataset(const CorpusDataset&) = delete;                 // Disable copy
	CorpusDataset& operator=(const CorpusDataset&) = delete;      // Disable assignment
};
