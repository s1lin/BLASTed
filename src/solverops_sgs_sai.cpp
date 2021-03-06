/** \file
 * \brief Implementation of SGS (I)SAI
 */

#include <boost/align/aligned_alloc.hpp>
#include <Eigen/Dense>
#include "solverops_sgs_sai.hpp"

namespace blasted {

using boost::alignment::aligned_alloc;
using boost::alignment::aligned_free;

template <typename scalar, typename index, int bs, StorageOptions stopt>
BSGS_SAI<scalar,index,bs,stopt>::BSGS_SAI(const bool full_spai)
	: fullsai{full_spai},
	  saiL.nbrows{0}, saiL.browptr{nullptr}, saiL.bcolind{nullptr},
	saiL.diagind{nullptr}, saiL.vals{nullptr},
	saiU.nbrows{0}, saiU.browptr{nullptr}, saiU.bcolind{nullptr},
	saiU.diagind{nullptr}, saiU.vals{nullptr}
{ }

template <typename scalar, typename index, int bs, StorageOptions stopt>
BSGS_SAI<scalar,index,bs,stopt>::~BSGS_SAI()
{
	aligned_free(saiL.browptr);
	aligned_free(saiL.bcolind);
	aligned_free(saiL.vals);
	aligned_free(saiL.diagind);
	aligned_free(saiU.browptr);
	aligned_free(saiU.bcolind);
	aligned_free(saiU.vals);
	aligned_free(saiU.diagind);
}

template <typename scalar, typename index, int bs, StorageOptions stopt>
void BSGS_SAI<scalar,index,bs,stopt>::compute()
{
	if(mat.nbrows == 0 || mat.browptr == nullptr)
		throw std::runtime_error("BSGS_SAI: Matrix not set!");

	/// Assumes sparsity pattern does not change once set.
	if(!sai.browptr)
	{
		initialize();
	}

	// Compute SAI or ISAI
#pragma omp parallel for default(shared)
	for(index irow = 0; irow < mat.nbrows; irow++)
	{
		compute_SAI_lower(irow);
		compute_SAI_upper(irow);
	}
}

template <typename scalar, typename index, int bs, StorageOptions stopt>
void BSGS_ISAI<scalar,index,bs,stopt>::apply(const scalar *const x, scalar *const __restrict y) const
{
	//bsr_matrix_apply<scalar,index,bs,stopt>(sai, x, y);
}

template <typename scalar, typename index, int bs, StorageOptions stopt>
void BSGS_ISAI<scalar,index,bs,stopt>::apply_relax(const scalar *const x, scalar *const __restrict y) const
{
	throw std::runtime_error("BSGS_SAI does not have relaxation!");
}

template <typename scalar, typename index, int bs, StorageOptions stopt>
void BSGS_ISAI<scalar,index,bs,stopt>::compute_SAI_lower(const index irow)
{
	// compute number of columns of A that are needed for this row of M
}

template <typename scalar, typename index, int bs, StorageOptions stopt>
void BSGS_SAI<scalar,index,bs,stopt>::initialize()
{
	// allocate memory for storing approximate inverses

	saiL.nbrows = mat.nbrows;
	saiU.nbrows = mat.nbrows;
	saiL.browptr = (index*)aligned_alloc(CACHE_LINE_LEN, (mat.nbrows+1)*sizeof(index));
	saiU.browptr = (index*)aligned_alloc(CACHE_LINE_LEN, (mat.nbrows+1)*sizeof(index));

	index lnnz = 0, unnz = 0;
	saiL.browptr[0] = 0; saiU.browptr[0] = 0;

	for(index irow = 0; irow < mat.nbrows; irow++)
	{
		for(index jj = mat.browptr[irow]; jj <= mat.diagind[irow]; jj++) {
			lnnz++;
		}
		saiL.browptr[irow+1] = lnnz;

		for(index jj = mat.diagind[irow]; jj < mat.browptr[irow+1]; jj++) {
			unnz++;
		}
		saiU.browptr[irow+1] = unnz;
	}

	if(saiL.browptr[saiL.nbrows] + saiU.browptr[saiU.nbrows] - mat.nbrows != mat.browptr[mat.nbrows])
		throw std::runtime_error("NNZs of lower and/or upper SAIs are wrong!");

	saiL.bcolind = (index*)aligned_alloc(CACHE_LINE_LEN, saiL.browptr[saiL.nbrows]*sizeof(index));
	saiL.vals = (scalar*)aligned_alloc(CACHE_LINE_LEN, saiL.browptr[saiL.nbrows]*bs*bs*sizeof(scalar));
	saiL.diagind = (index*)aligned_alloc(CACHE_LINE_LEN, saiL.nbrows*sizeof(index));
	saiU.bcolind = (index*)aligned_alloc(CACHE_LINE_LEN, saiU.browptr[saiU.nbrows]*sizeof(index));
	saiU.vals = (scalar*)aligned_alloc(CACHE_LINE_LEN, saiU.browptr[saiU.nbrows]*bs*bs*sizeof(scalar));
	saiU.diagind = (index*)aligned_alloc(CACHE_LINE_LEN, saiU.nbrows*sizeof(index));

	lnnz = 0; unnz = 0;
	for(index irow = 0; irow < mat.nbrows; irow++)
	{
		for(index jj = mat.browptr[irow]; jj <= mat.diagind[irow]; jj++) {
			saiL.bcolind[lnnz] = mat.bcolind[jj];
			lnnz++;
		}

		saiL.diagind[irow] = lnnz-1;
		saiU.diagind[irow] = unnz;

		for(index jj = mat.diagind[irow]; jj < mat.browptr[irow+1]; jj++) {
			saiU.bcolind[unnz] = mat.bcolind[jj];
			unnz++;
		}
	}

	compute_pattern();
}

}
