/** \file relaxation_async_sgs.hpp
 * \brief Asynchronous symmetric Gauss-Seidel relaxation
 * \author Aditya Kashi
 * \date 2018-05
 */

#ifndef BLASTED_RELAXATION_ASYNCSGS
#define BLASTED_RELAXATION_ASYNCSGS

#include "solverops_jacobi.hpp"

namespace blasted {

/// Asynchronous block SGS relaxation
/** Fully asynchronous relaxation in which every "sweep" is followed by a sweep in the reverse
 * direction. Unlike the ABSGS preconditioner, this operation does not use `-blasted_async_sweeps';
 * this is fully asychronous relaxation.
 */
template<typename scalar, typename index, int bs, StorageOptions stor>
class AsyncBlockSGS_Relaxation : public BJacobiSRPreconditioner<scalar,index,bs,stor>
{
public:
	AsyncBlockSGS_Relaxation(const int threadchunksize);

	/// Carry out chaotic block SGS relaxation
	/** For this solver, tolerance checking is never done irrespective of
	 * \ref Precontitioner::setApplyParams.
	 * \param b The right hand side in Ax=b
	 * \param x The solution vector, initially containing the initial guess
	 */
	void apply(const scalar *const b, scalar *const __restrict x) const;

	/// Does nothing but throw an exception
	void apply_relax(const scalar *const x, scalar *const __restrict y) const;

protected:
	using SRPreconditioner<scalar,index>::mat;
	using BJacobiSRPreconditioner<scalar,index,bs,stor>::dblocks;
	using Preconditioner<scalar,index>::solveparams;

	using Blk = Block_t<scalar,bs,stor>;
	using Seg = Segment_t<scalar,bs>;
	
	const int thread_chunk_size;
};

/// Asynchronous SGS relaxation
/** Fully asynchronous relaxation in which every "sweep" is followed by a sweep in the reverse
 * direction. Unlike the ASGS preconditioner, this operation does not use `-blasted_async_sweeps';
 * this is fully asychronous relaxation.
 */
template<typename scalar, typename index>
class AsyncSGS_Relaxation : public JacobiSRPreconditioner<scalar,index>
{
public:
	AsyncSGS_Relaxation(const int threadchunksize);

	/// Carry out chaotic block SGS relaxation
	/** For this solver, tolerance checking is never done irrespective of
	 * \ref Precontitioner::setApplyParams.
	 * \param b The right hand side in Ax=b
	 * \param x The solution vector, initially containing the initial guess
	 */
	void apply(const scalar *const b, scalar *const __restrict x) const;

	/// Does nothing but throw an exception
	void apply_relax(const scalar *const x, scalar *const __restrict y) const;

protected:
	using SRPreconditioner<scalar,index>::mat;
	using JacobiSRPreconditioner<scalar,index>::dblocks;
	using Preconditioner<scalar,index>::solveparams;

	const int thread_chunk_size;
};
	
}

#endif
