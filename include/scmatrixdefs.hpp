/** \file scmatrixdefs.hpp
 * \brief Some definitions for everything depending on sparse-column storage
 * \author Aditya Kashi
 */

#ifndef BLASTED_SCMATRIXDEFS_H
#define BLASTED_SCMATRIXDEFS_H

#include <limits>
#include "srmatrixdefs.hpp"

namespace blasted {

/// A compressed sparse block-column square matrix
template <typename scalar, typename index>
struct RawBSCMatrix
{
	static_assert(std::numeric_limits<index>::is_integer, "Integer index type required!");
	static_assert(std::numeric_limits<index>::is_signed, "Signed index type required!");

	/// Array of pointers into \ref browind that point to first entries of (block-)columns
	index * bcolptr;
	/// Array of row indices of each non-zero entry, stored in the same order as \ref vals
	index * browind;
	/// Array of non-zero values
	scalar * vals;
	/// Pointers into \ref browind pointing to diagonal entries in each column
	index * diagind;
	/// Number of (block-)columns
	index nbcols;
};

/// An almost-immutable compressed sparse block-column square matrix
/** A const object of this time is immutable.
 */
template <typename scalar, typename index>
struct CRawBSCMatrix
{
	static_assert(std::numeric_limits<index>::is_integer, "Integer index type required!");
	static_assert(std::numeric_limits<index>::is_signed, "Signed index type required!");

	/// Array of pointers into \ref browind that point to first entries of (block-)columns
	const index * bcolptr;
	/// Array of row indices of each non-zero entry, stored in the same order as \ref vals
	const index * browind;
	/// Array of non-zero values
	const scalar * vals;
	/// Pointers into \ref browind pointing to diagonal entries in each column
	const index * diagind;
	/// Number of (block-)columns
	index nbcols;
};

/// Converts a (block-) sparse-row matrix to a (block-) sparse-column matrix
/** Assumes a square matrix.
 */
template <typename scalar, typename index, int bs>
void convert_BSR_to_BSC(const CRawBSRMatrix<scalar,index> *const rmat,
                        CRawBSCMatrix<scalar,index> *const cscmat);

/// Converts a 0-based (block-) sparse-row matrix to a 1-based (block-) sparse-column matrix
/** Assumes a square matrix.
 * Convenient for using the resulting matrix with Fortran subroutines.
 */
template <typename scalar, typename index, int bs>
CRawBSCMatrix<scalar,index> convert_BSR_to_BSC_1based(const CRawBSRMatrix<scalar,index> *const rmat);

/// Frees aligned storage
template <typename scalar, typename index>
void alignedDestroyRawBSCMatrix(RawBSCMatrix<scalar,index>& cmat);

/// Frees aligned storage
template <typename scalar, typename index>
void alignedDestroyCRawBSCMatrix(CRawBSCMatrix<scalar,index>& cmat);

}
#endif
