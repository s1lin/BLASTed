/** \file
 * \brief Implementation for calculation of index lists needed for ILU
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

#include <numeric>
#include <iostream>
#include "ilu_pattern.hpp"
#include "helper_algorithms.hpp"

namespace blasted {

/// Exclusive prefix-scan over a vector
/** We assume the first element of the vector is zero, and that the actual element starts at position 1.
 */
template <typename T>
static void exclusive_scan(std::vector<T>& v);

template <typename scalar, typename index>
ILUPositions<index> compute_ILU_positions_CSR_CSR(const CRawBSRMatrix<scalar,index> *const mat)
{
	ILUPositions<index> pos;
	const index pattern_size = mat->browptr[mat->nbrows];

	std::vector<size_t> numpos(pattern_size,0);
	pos.posptr.resize(pattern_size+1);
	pos.posptr[0] = 0;

	// compute number of indices that need to stored for each nonzero
	for(index irow = 0; irow < mat->nbrows; irow++) {
		for(index j = mat->browptr[irow]; j < mat->browptr[irow+1]; j++)
		{
			if(irow > mat->bcolind[j])
			{
				for(index k = mat->browptr[irow]; 
				    (k < mat->browptr[irow+1]) && (mat->bcolind[k] < mat->bcolind[j]); 
				    k++  ) 
				{
					index ipos = -1;
					internal::inner_search(mat->bcolind, mat->diagind[mat->bcolind[k]], 
					                       mat->browptr[mat->bcolind[k]+1], mat->bcolind[j], &ipos);

					if(ipos > -1)
						numpos[j]++;
				}
			}
			else
			{
				// u_ij

				for(index k = mat->browptr[irow]; 
				    (k < mat->browptr[irow+1]) && (mat->bcolind[k] < irow); k++) 
				{
					index ipos = -1;

					/* search for column index mat->bcolind[j], 
					 * between the diagonal index of row mat->bcolind[k] 
					 * and the last index of row mat->bcolind[k]
					 */
					internal::inner_search(mat->bcolind, mat->diagind[mat->bcolind[k]], 
					                       mat->browptr[mat->bcolind[k]+1], mat->bcolind[j], &ipos);

					if(ipos > -1)
						numpos[j]++;
				}
			}

			pos.posptr[j+1] = numpos[j];
		}
	}

	const size_t totallen = std::accumulate(numpos.begin(), numpos.end(), 0);
	exclusive_scan(pos.posptr);

	//assert(static_cast<size_t>(pos.posptr[pattern_size-1]) == totallen);

	pos.lowerp.resize(totallen);
	pos.upperp.resize(totallen);

	for(index irow = 0; irow < mat->nbrows; irow++) {
		for(index j = mat->browptr[irow]; j < mat->browptr[irow+1]; j++)
		{
			if(irow > mat->bcolind[j])
			{
				for(index k = mat->browptr[irow]; 
				    (k < mat->browptr[irow+1]) && (mat->bcolind[k] < mat->bcolind[j]); 
				    k++  ) 
				{
					index ipos = -1;
					internal::inner_search(mat->bcolind, mat->diagind[mat->bcolind[k]], 
					                       mat->browptr[mat->bcolind[k]+1], mat->bcolind[j], &ipos);

					if(ipos != -1)
					{
						pos.lowerp[pos.posptr[j]] = k;
						pos.upperp[pos.posptr[j]] = ipos;
					}
				}
			}
			else
			{
				// u_ij

				for(index k = mat->browptr[irow];
				    (k < mat->browptr[irow+1]) && (mat->bcolind[k] < irow); k++) 
				{
					index ipos = -1;

					/* search for column index mat->bcolind[j], 
					 * between the diagonal index of row mat->bcolind[k] 
					 * and the last index of row mat->bcolind[k]
					 */
					internal::inner_search(mat->bcolind, mat->diagind[mat->bcolind[k]], 
					                       mat->browptr[mat->bcolind[k]+1], mat->bcolind[j], &ipos);

					if(ipos != -1)
					{
						pos.lowerp[pos.posptr[j]] = k;
						pos.upperp[pos.posptr[j]] = ipos;
					}
				}
			}
		}
	}

	std::cout << "  Computed required locations in L and U factors.\n";
	return pos;
}

// Serial
template <typename T>
void exclusive_scan(std::vector<T>& v)
{
	for(size_t i = 1; i < v.size(); i++)
	{
		v[i] += v[i-1];
	}
}

template ILUPositions<int> compute_ILU_positions_CSR_CSR(const CRawBSRMatrix<double,int> *const mat);

}
