/** \file blockmatrices.ipp
 * \brief Implementation of block matrix methods,
 * including the specialized set of methods for block size 1.
 * \author Aditya Kashi
 * \date 2017-08
 */

template <typename scalar, typename index, int bs>
BSRMatrix<scalar,index,bs>::BSRMatrix(const index n_brows,
		const index *const bcinds, const index *const brptrs,
		const short n_buildsweeps, const short n_applysweeps)
	: vals(nullptr), isAllocVals(true), nbrows{n_brows},
	  nbuildsweeps{n_buildsweeps}, napplysweeps{n_applysweeps}, thread_chunk_size{500}
{
	constexpr int bs2 = bs*bs;
	browptr = new index[nbrows+1];
	bcolind = new index[brptrs[nbrows]];
	diagind = new index[nbrows];
	vals = new scalar[brptrs[nbrows]*bs2];
	for(index i = 0; i < nbrows+1; i++)
		browptr[i] = brptrs[i];
	for(index i = 0; i < brptrs[nbrows]; i++)
		bcolind[i] = bcinds[i];

	// set diagonal blocks' locations
	for(index irow = 0; irow < nbrows; irow++) {
		for(index j = browptr[irow]; j < browptr[irow+1]; j++)
			if(bcolind[j] == irow) {
				diagind[irow] = j;
				break;
			}
	}
}

template <typename scalar, typename index, int bs>
BSRMatrix<scalar, index, bs>::~BSRMatrix()
{
	if(isAllocVals)
		delete [] vals;
	if(bcolind)
		delete [] bcolind;
	if(browptr)
		delete [] browptr;
	if(diagind)
		delete [] diagind;
	bcolind = browptr = diagind = nullptr;
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::setAllZero()
{
#pragma omp parallel for simd default(shared)
	for(index i = 0; i < browptr[nbrows]; i++)
		vals[i] = 0;
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::setDiagZero()
{
	constexpr int bs2 = bs*bs;

#pragma omp parallel for default(shared)
	for(index irow = 0; irow < nbrows; irow++)
#pragma omp simd
		for(index jj = diagind[irow]*bs2; jj < (diagind[irow]+1)*bs2; jj++)
			vals[jj] = 0;
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::submitBlock(const index starti, const index startj,
		const scalar *const buffer, const long param1, const long param2) 
{
	bool found = false;
	const index startr = starti/bs, startc = startj/bs;
	constexpr int bs2 = bs*bs;
	for(index j = browptr[startr]; j < browptr[startr+1]; j++) {
		if(bcolind[j] == startc) {
			for(int k = 0; k < bs2; k++)
				vals[j*bs2 + k] = buffer[k];
			found = true;
		}
	}
	if(!found)
		std::cout << "! BSRMatrix: submitBlock: Block not found!!\n";
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::updateDiagBlock(const index starti,
		const scalar *const buffer, const long param)
{
	constexpr int bs2 = bs*bs;
	const index startr = starti/bs;
	const index pos = diagind[startr];
	for(int k = 0; k < bs2; k++)
#pragma omp atomic update
		vals[pos*bs2 + k] += buffer[k];
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::updateBlock(const index starti, const index startj,
		const scalar *const buffer, const long param1, const long param2)
{
	bool found = false;
	const index startr = starti/bs, startc = startj/bs;
	constexpr int bs2 = bs*bs;

	for(index j = browptr[startr]; j < browptr[startr+1]; j++) {
		if(bcolind[j] == startc) {
			for(int k = 0; k < bs2; k++)
			{
#pragma omp atomic update
				vals[j*bs2 + k] += buffer[k];
			}
			found = true;
		}
	}
	if(!found)
		std::cout << "! BSRMatrix: submitBlock: Block not found!!\n";
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::apply(const scalar a, const scalar *const xx,
                                       scalar *const __restrict yy) const
{
	Eigen::Map<const Vector<scalar>> x(xx, nbrows*bs);
	Eigen::Map<Vector<scalar>> y(yy, nbrows*bs);
	Eigen::Map<const Matrix<scalar,Dynamic,bs,RowMajor>> data(vals, nbrows, bs);

#pragma omp parallel for default(shared)
	for(index irow = 0; irow < nbrows; irow++)
	{
		y.SEG<bs>(irow*bs) = Vector<scalar>::Zero(bs);

		// loop over non-zero blocks of this block-row
		for(index jj = browptr[irow]; jj < browptr[irow+1]; jj++)
		{
			// multiply the blocks with corresponding sub-vectors
			const index jcol = bcolind[jj];
			y.SEG<bs>(irow*bs).noalias() 
				+= a * data.BLK<bs,bs>(jj*bs,0) * x.SEG<bs>(jcol*bs);
			
			/*for(int i = 0; i < bs; i++)
				for(int j = 0; j < bs; j++)
					y[irow*bs+i] += a * data[jj*bs2 + i*bs+j] * x[jcol*bs+j];*/
		}
	}
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::gemv3(const scalar a, const scalar *const __restrict__ xx, 
		const scalar b, const scalar *const yy, scalar *const zz) const
{
	Eigen::Map<const Vector<scalar>> x(xx, nbrows*bs);
	Eigen::Map<const Vector<scalar>> y(yy, nbrows*bs);
	Eigen::Map<Vector<scalar>> z(zz, nbrows*bs);
	Eigen::Map<const Matrix<scalar,Dynamic,bs,RowMajor>> data(vals, nbrows, bs);

#pragma omp parallel for default(shared)
	for(index irow = 0; irow < nbrows; irow++)
	{
		z.SEG<bs>(irow*bs) = b * y.SEG<bs>(irow*bs);

		// loop over non-zero blocks of this block-row
		for(index jj = browptr[irow]; jj < browptr[irow+1]; jj++)
		{
			const index jcol = bcolind[jj];
			z.SEG<bs>(irow*bs).noalias() += 
				a * data.BLK<bs,bs>(jj*bs,0) * x.SEG<bs>(jcol*bs);
		}
	}
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::precJacobiSetup()
{
	if(dblocks.size() <= 0) {
		dblocks.resize(nbrows*bs,bs);
#if DEBUG==1
		std::cout << " BSRMatrix: precJacobiSetup(): Allocating.\n";
#endif
	}
	
	Eigen::Map<const Matrix<scalar,Dynamic,bs,RowMajor>> data(vals, nbrows, bs);

#pragma omp parallel for default(shared)
	for(index irow = 0; irow < nbrows; irow++)
		dblocks.BLK<bs,bs>(irow*bs,0) = data.BLK<bs,bs>(diagind[irow]*bs,0).inverse();
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::precJacobiApply(const scalar *const rr, 
                                                 scalar *const __restrict__ zz) const
{
	Eigen::Map<const Vector<scalar>> r(rr, nbrows*bs);
	Eigen::Map<Vector<scalar>> z(zz, nbrows*bs);
	Eigen::Map<const Matrix<scalar,Dynamic,bs,RowMajor>> data(vals, nbrows, bs);

#pragma omp parallel for default(shared)
	for(index irow = 0; irow < nbrows; irow++)
		z.SEG<bs>(irow*bs).noalias() = dblocks.BLK<bs,bs>(irow*bs,0) * r.SEG<bs>(irow*bs);
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::allocTempVector()
{
	ytemp.resize(nbrows*bs,1);
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::precSGSApply(const scalar *const rr, 
                                              scalar *const __restrict__ zz) const
{
	Eigen::Map<const Vector<scalar>> r(rr, nbrows*bs);
	Eigen::Map<Vector<scalar>> z(zz, nbrows*bs);
	Eigen::Map<const Matrix<scalar,Dynamic,bs,RowMajor>> data(vals, nbrows, bs);
	constexpr int bs2 = bs*bs;

	for(short isweep = 0; isweep < napplysweeps; isweep++)
	{
		// forward sweep ytemp := D^(-1) (r - L ytemp)

#pragma omp parallel for default(shared) schedule(dynamic, thread_chunk_size)
		for(index irow = 0; irow < nbrows; irow++)
		{
			Matrix<scalar,bs,1> inter = Matrix<scalar,bs,1>::Zero();

			for(index jj = browptr[irow]; jj < diagind[irow]; jj++)
				inter += data.BLK<bs,bs>(jj*bs,0)*ytemp.SEG<bs>(bcolind[jj]);

			ytemp.SEG<bs>(irow) = dblocks.BLK<bs,bs>(irow*bs,0) 
			                                          * (r.SEG<bs>(irow) - inter);
		}
	}

	for(short isweep = 0; isweep < napplysweeps; isweep++)
	{
		// backward sweep z := D^(-1) (D y - U z)

#pragma omp parallel for default(shared) schedule(dynamic, thread_chunk_size)
		for(index irow = 0; irow < nbrows; irow++)
		{
			Matrix<scalar,bs,1> inter = Matrix<scalar,bs,1>::Zero();
			
			// compute U z
			for(index jj = diagind[irow]; jj < browptr[irow+1]; jj++)
				inter += data.BLK<bs,bs>(jj*bs,0) * z.SEG<bs>(bcolind[jj]*bs);

			// compute z = D^(-1) (D y - U z) for the irow-th block-segment of z
			z.SEG<bs>(irow*bs) = dblocks.BLK<bs,bs>(irow*bs2,0) 
				* ( data.BLK<bs,bs>(diagind[irow]*bs,0)*ytemp.SEG<bs>(irow*bs) - inter );
		}
	}
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::precILUSetup()
{
	Eigen::Map<const Matrix<scalar,Dynamic,bs,RowMajor>> data(vals, nbrows, bs);
	// TODO
}

template <typename scalar, typename index, int bs>
void BSRMatrix<scalar,index,bs>::precILUApply(const scalar *const r, 
                                              scalar *const __restrict__ z) const
{
	// TODO
}



template <typename scalar, typename index>
BSRMatrix<scalar,index,1>::BSRMatrix(const index n_brows,
		const index *const bcinds, const index *const brptrs,
		const short n_buildsweeps, const short n_applysweeps)
	: vals(nullptr), isAllocVals(true), nbrows{n_brows}, 
	  dblocks(nullptr), iludata(nullptr), ytemp(nullptr),
	  nbuildsweeps{n_buildsweeps}, napplysweeps{n_applysweeps}, thread_chunk_size{800}
{
	browptr = new index[nbrows+1];
	bcolind = new index[brptrs[nbrows]];
	diagind = new index[nbrows];
	vals = new scalar[brptrs[nbrows]];
	for(index i = 0; i < nbrows+1; i++)
		browptr[i] = brptrs[i];
	for(index i = 0; i < brptrs[nbrows]; i++)
		bcolind[i] = bcinds[i];

	// set diagonal blocks' locations
	for(index irow = 0; irow < nbrows; irow++) {
		for(index j = browptr[irow]; j < browptr[irow+1]; j++)
			if(bcolind[j] == irow) {
				diagind[irow] = j;
				break;
			}
	}
}

template <typename scalar, typename index>
BSRMatrix<scalar,index,1>::BSRMatrix(const index nrows, const index *const brptrs,
		const index *const bcinds, const scalar *const values,
		const short n_buildsweeps, const short n_applysweeps)
	: vals(values), isAllocVals(false), bcolind(bcinds), browptr(brptrs), nbrows{nrows},
	  dblocks(nullptr), iludata(nullptr), ytemp(nullptr),
	  nbuildsweeps{n_buildsweeps}, napplysweeps{n_applysweeps}, thread_chunk_size{800}
{
	// set diagonal blocks' locations
	for(index irow = 0; irow < nbrows; irow++) {
		for(index j = browptr[irow]; j < browptr[irow+1]; j++)
			if(bcolind[j] == irow) {
				diagind[irow] = j;
				break;
			}
	}
}

template <typename scalar, typename index>
BSRMatrix<scalar,index,1>::~BSRMatrix()
{
	if(isAllocVals)
		delete [] vals;
	if(bcolind)
		delete [] bcolind;
	if(browptr)
		delete [] browptr;
	if(diagind)
		delete [] diagind;
	if(dblocks)
		delete [] dblocks;
	if(iludata)
		delete [] iludata;
	if(ytemp)
		delete [] ytemp;
	bcolind = browptr = diagind = nullptr;
	vals = dblocks = iludata = ytemp = nullptr;
}

template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::setAllZero()
{
#pragma omp parallel for simd default(shared)
	for(index i = 0; i < browptr[nbrows]; i++)
		vals[i] = 0;
}

template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::setDiagZero()
{
#pragma omp parallel for default(shared)
	for(index irow = 0; irow < nbrows; irow++)
		vals[diagind[irow]] = 0;
}

/** \warning It is assumed that locations corresponding to indices of entries to be added
 * already exist. If that's not the case, Bad Things (TM) will happen.
 */
template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::submitBlock(const index starti, const index startj,
		const scalar *const buffer, const long bsi, const long bsj)
{
	for(index irow = starti; irow < starti+bsi; irow++)
	{
		long k = 0;
		index locrow = irow-starti;
		for(index jj = browptr[irow]; jj < browptr[irow+1]; jj++)
		{
			if(bcolind[jj] < startj)
				continue;
			if(k == bsj) 
				break;
#ifdef DEBUG
			if(bcolind[jj] != startj+k)
				std::cout << "!  BSRMatrix<1>: updateBlock: Invalid block!!\n";
#endif
			vals[jj] = buffer[locrow*bsi+k];
			k++;
		}
	}
}

/** \warning It is assumed that locations corresponding to indices of entries to be added
 * already exist. If that's not the case, Bad Things (TM) will happen.
 */
template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::updateDiagBlock(const index starti,
		const scalar *const buffer, const long bs)
{
	// update the block row-wise
	for(index irow = starti; irow < starti+bs; irow++)
	{
		long k = 0;
		index locrow = irow-starti;
		for(index jj = browptr[irow]; jj < browptr[irow+1]; jj++)
		{
			if(bcolind[jj] < starti) 
				continue;
			if(k == bs) 
				break;
#ifdef DEBUG
			if(bcolind[jj] != starti+k)
				std::cout << "!  BSRMatrix<1>: updateDiagBlock: Invalid block!!\n";
#endif
#pragma omp atomic update
			vals[jj] += buffer[locrow*bs+k];
			k++;
		}
	}
}

/** \warning It is assumed that locations corresponding to indices of entries to be added
 * already exist. If that's not the case, Bad Things (TM) will happen.
 */
template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::updateBlock(const index starti, const index startj,
		const scalar *const buffer, const long bsi, const long bsj)
{
	for(index irow = starti; irow < starti+bsi; irow++)
	{
		long k = 0;
		index locrow = irow-starti;
		for(index jj = browptr[irow]; jj < browptr[irow+1]; jj++)
		{
			if(bcolind[jj] < startj)
				continue;
			if(k == bsj) 
				break;
#ifdef DEBUG
			if(bcolind[jj] != startj+k)
				std::cout << "!  BSRMatrix<1>: updateBlock: Invalid block!!\n";
#endif
#pragma omp atomic update
			vals[jj] += buffer[locrow*bsi+k];
			k++;
		}
	}
}

template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::apply(const scalar a, const scalar *const xx,
                                       scalar *const __restrict yy) const
{
#pragma omp parallel for default(shared)
	for(index irow = 0; irow < nbrows; irow++)
	{
		yy[irow] = 0;

		for(index jj = browptr[irow]; jj < browptr[irow+1]; jj++)
		{
			yy[irow] += a * vals[jj] * xx[bcolind[jj]];
		}
	}
}

template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::gemv3(const scalar a, const scalar *const __restrict__ xx, 
		const scalar b, const scalar *const yy, scalar *const zz) const
{
#pragma omp parallel for default(shared)
	for(index irow = 0; irow < nbrows; irow++)
	{
		zz[irow] = b * yy[irow];

		for(index jj = browptr[irow]; jj < browptr[irow+1]; jj++)
		{
			zz[irow] += a * vals[jj] * xx[bcolind[jj]];
		}
	}
}

template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::precJacobiSetup()
{
	if(!dblocks) {
		dblocks = new scalar[browptr[nbrows]];
#if DEBUG==1
		std::cout << " BSRMatrix<1>: precJacobiSetup(): Allocating.\n";
#endif
	}

#pragma omp parallel for simd default(shared)
	for(index irow = 0; irow < nbrows; irow++)
		dblocks[irow] = 1.0/vals[diagind[irow]];
}

template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::precJacobiApply(const scalar *const rr, 
                                                 scalar *const __restrict zz) const
{
#pragma omp parallel for simd default(shared)
	for(index irow = 0; irow < nbrows; irow++)
		zz[irow] = dblocks[irow] * rr[irow];
}

template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::allocTempVector()
{
	if(ytemp)
		std::cout << "!  BSRMatrix<1>: allocTempVector(): temp vector is already allocated!\n";
	else
		ytemp = new scalar[browptr[nbrows]];
}

template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::precSGSApply(const scalar *const rr, 
                                              scalar *const __restrict zz) const
{
	for(short isweep = 0; isweep < napplysweeps; isweep++)
	{
		// forward sweep ytemp := D^(-1) (r - L ytemp)

#pragma omp parallel for default(shared) schedule(dynamic, thread_chunk_size)
		for(index irow = 0; irow < nbrows; irow++)
		{
			scalar inter = 0;

			for(index jj = browptr[irow]; jj < diagind[irow]; jj++)
				inter += vals[jj]*ytemp[bcolind[jj]];

			ytemp[irow] = dblocks[irow] * (rr[irow] - inter);
		}
	}

	for(short isweep = 0; isweep < napplysweeps; isweep++)
	{
		// backward sweep z := D^(-1) (D y - U z)

#pragma omp parallel for default(shared) schedule(dynamic, thread_chunk_size)
		for(index irow = 0; irow < nbrows; irow++)
		{
			scalar inter = 0;
			
			// compute U z
			for(index jj = diagind[irow]; jj < browptr[irow+1]; jj++)
				inter += vals[jj] * zz[bcolind[jj]];

			// compute z = D^(-1) (D y - U z) for the irow-th block-segment of z
			zz[irow] = dblocks[irow] * ( vals[diagind[irow]]*ytemp[irow] - inter );
		}
	}
}

template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::precILUSetup()
{
	// TODO
}

template <typename scalar, typename index>
void BSRMatrix<scalar,index,1>::precILUApply(const scalar *const r, 
                                              scalar *const __restrict z) const
{
	// TODO
}

