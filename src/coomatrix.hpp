/** \file coomatrix.hpp
 * \brief Specifies coordinate matrix formats
 * \author Aditya Kashi
 * 
 * This file is part of BLASTed.
 *   BLASTed is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   BLASTed is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with BLASTed.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef COOMATRIX_H
#define COOMATRIX_H

#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include "blockmatrices.hpp"

namespace blasted {

/// Encodes matrix types in matrix market file format
enum MMMatrixType {GENERAL, SYMMETRIC, SKEWSYMMETRIC, HERMITIAN};
/// Encodes matrix storage type in a matrix market file
enum MMStorageType {COORDINATE, ARRAY};
/// Encodes the scalar type of a matrix in a matrix market file
enum MMScalarType {REAL, COMPLEX, INTEGER, PATTERN};

/// Information contained in the Matrix Market format's first line
/** A description of the format is given [here](http://math.nist.gov/MatrixMarket/formats.html).
 */
struct MMDescription {
	MMStorageType storagetype;
	MMScalarType scalartype;
	MMMatrixType matrixtype;
};

/// A triplet that encapsulates one entry of a coordinate matrix
template <typename scalar, typename index>
struct Entry {
	index rowind;
	index colind;
	scalar value;
};

/// A sparse matrix with entries stored in coordinate format
template <typename scalar, typename index>
class COOMatrix
{
public:
	COOMatrix();

	virtual ~COOMatrix();

	/// Reads a matrix from a file in Matrix Market format
	void readMatrixMarket(const std::string file);
	
	// Multiplies the matrix with a vector
	//void apply(const scalar a, const scalar *const x, scalar *const __restrict y) const;

	/// Converts to a compressed sparse row matrix
	void convertToCSR(BSRMatrix<scalar,index,1> *const cmat) const;

	/// Converts to a compressed sparse block-row (BSR) matrix 
	/** The block size is given by the template parameter bs.
	 */
	template<int bs>
	void convertToBSR(BSRMatrix<scalar,index,bs> *const bmat) const;

protected:

	/// Returns a [description](\ref MMDescription) of the matrix if it's in Matrix Market format
	MMDescription getMMDescription(std::ifstream& fin);

	/// Returns a vector containing size information of a matrix in a Matrix Market file
	std::vector<index> getSizeFromMatrixMarket(
			std::ifstream& fin,                   ///< Opened file stream to read from
			const MMDescription& descr            ///< Matrix description
		);

	std::vector<Entry<scalar,index>> entries;     ///< Stored entries of the matrix
	index nnz;                                    ///< Number of nonzeros
	index nrows;                                  ///< Number of rows
	index ncols;                                  ///< Number of columns

	std::vector<index> rowptr;                    ///< Vector of row pointers into \ref entries
};

#include "coomatrix.ipp"

}

#endif
