/** \file
 * \brief Level-scheduled Gauss-Seidel iterations
 * \author Aditya Kashi
 * \date 2019-02
 */

#ifndef BLASTED_SOLVEROPS_LEVELS_SGS_H
#define BLASTED_SOLVEROPS_LEVELS_SGS_H

#include "solverops_jacobi.hpp"

namespace blasted {

template <typename scalar, typename index, int bs, StorageOptions stor>
class Level_BSGS : public BJacobiSRPreconditioner<scalar,index,bs,stor>
{
public:
	Level_BSGS();

	bool relaxationAvailable() const { return true; }

	/// Compute the preconditioner
	void compute();

	/// To apply the preconditioner
	void apply(const scalar *const x, scalar *const __restrict y) const;

	/// Carry out a relaxation solve
	void apply_relax(const scalar *const x, scalar *const __restrict y) const;

protected:
	using Preconditioner<scalar,index>::solveparams;
	using SRPreconditioner<scalar,index>::mat;
	using BJacobiSRPreconditioner<scalar,index,bs,stor>::dblocks;

	using Blk = Block_t<scalar,bs,stor>;
	using Seg = Segment_t<scalar,bs>;
	
	/// Temporary storage for the result of the forward Gauss-Seidel sweep
	mutable scalar *ytemp;
};

}

#endif
