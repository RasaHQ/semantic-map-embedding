
#include <fstream>
#include <iostream>
#include "data.hpp"
#include "utils.hpp"


CorpusDataset::CorpusDataset(const std::string& filename)
{
	std::cout << "Load corpus data from " << filename << std::endl;
	if (!file_exists(filename))
		std::__throw_runtime_error("File does not exist");

	std::fstream file(filename, std::ios::in | std::ios::binary);

	auto* buffer_1byte = new uint8_t[1];
	auto* buffer_2byte = new uint16_t[1];
	auto* buffer_4byte = new uint32_t[1];
	auto* buffer_8byte = new uint64_t[1];

	// Read format version
	file.read((char*)buffer_1byte, sizeof(uint8_t));
	auto format_version = *buffer_1byte;
	switch (format_version)
	{
	case 2:
		this->_has_weights = true;
		break;
	case 3:
		this->_has_weights = false;
		break;	
	default:
		std::__throw_runtime_error("Expected file format version 2 or 3");
	}

	// Read total number of entries in the matrix in this file
	file.read((char*)buffer_8byte, sizeof(uint64_t));
	if (*buffer_8byte > MAX_INDEX_POINTER_SIZE)
		std::__throw_runtime_error("Too many entries in training data");
	this->num_non_zero = buffer_8byte[0];

	// Read matrix size
	file.read((char*)buffer_4byte, sizeof(uint32_t));
	this->num_rows = int(buffer_4byte[0]);	
	file.read((char*)buffer_4byte, sizeof(uint32_t));
	this->num_cols = int(buffer_4byte[0]);

	// Read the data
	this->indices.reserve(this->num_non_zero);
	if (this->has_weights())
		this->weights.reserve(this->num_non_zero);
	this->index_pointers.reserve(this->num_rows + 1);
	this->index_pointers[0] = 0;
	IndexPointerType index_pointer = 0;
	IndexType entires_in_row;
	for (IndexPointerType row = 0; row < this->num_rows; ++row) 
	{
		// Read number of entries in this row
		file.read((char*)buffer_4byte, sizeof(IndexType));
		entires_in_row = reinterpret_cast<IndexType>(buffer_4byte[0]);


		// Set index pointer for beginning of the next row (or end of the data)
		this->index_pointers[row + 1] = index_pointer + entires_in_row;

		const IndexPointerType index_pointer_start = index_pointer;

		// Read the entries in this row
		for (IndexType entry = 0; entry < entires_in_row; ++entry)
		{
				file.read((char*)buffer_4byte, sizeof(IndexType));
				IndexType index = reinterpret_cast<IndexType>(buffer_4byte[0]);
				this->indices[index_pointer] = index; 
				index_pointer++;
		}

		if (!this->has_weights())
			continue;

		// Read the weights in this row
		index_pointer = index_pointer_start;
		for (IndexType entry = 0; entry < entires_in_row; ++entry)
		{
				file.read((char*)buffer_1byte, sizeof(WeightType));
				WeightType weight = reinterpret_cast<WeightType>(buffer_1byte[0]);
				this->weights[index_pointer] = weight; 
				index_pointer++;
		}
	}

	file.close();
	delete [] buffer_8byte;
	delete [] buffer_4byte;
	delete [] buffer_2byte;
}


IndexType BinarySparseMatrix::min_word_index_to_avoid_empty_row()
{
	IndexType max_first_word_index = 0;

	for (IndexPointerType row = 0; row < this->num_rows; ++row)
	{
		const IndexType* const x = this->indices_in_row(row);     // Address of the beginning of the array of columns-with-value-indices

		// We assume that word indices are sorted from small to large
		// so x[0] is the smallest word index in this row
		if (x[0] > max_first_word_index)
			max_first_word_index = x[0];
	}

	return max_first_word_index;
}


void BinarySparseMatrix::init_sum_of_squares()
{
	this->_sum_of_squares = new IndexType[this->num_rows];

	IndexType num_non_zero_entries_in_row;

	for (IndexPointerType row = 0; row < this->num_rows; ++row)
	{
		num_non_zero_entries_in_row = this->num_indices_in_row(row);
		if (this->has_weights())
		{
			auto* weights = this->weights_in_row(row);
			this->_sum_of_squares[row] = 0;
			for (IndexType i = 0; i < num_non_zero_entries_in_row; ++i)
				this->_sum_of_squares[row] += squared(weights[i]);
		}
		else
		{
			this->_sum_of_squares[row] = num_non_zero_entries_in_row;
		}
		
	}
}
