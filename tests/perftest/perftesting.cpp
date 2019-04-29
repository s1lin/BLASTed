/** \file
 * \brief Implementation of a thread-scaling test for BLASTed solvers on a given matrix
 */

#include <iomanip>
#include <blasted_petsc.h>
#include "perftesting.hpp"

/// Set -blasted_async_sweeps in the default Petsc options database and throw if not successful
static int set_blasted_sweeps(const int nbswp, const int naswp)
{
	// add option
	std::string value = std::to_string(nbswp) + "," + std::to_string(naswp);
	int ierr = PetscOptionsSetValue(NULL, "-blasted_async_sweeps", value.c_str()); CHKERRQ(ierr);

	// Check
	int checksweeps[2];
	int nmax = 2;
	PetscBool set = PETSC_FALSE;
	ierr = PetscOptionsGetIntArray(NULL,NULL,"-blasted_async_sweeps",checksweeps,&nmax,&set);
	CHKERRQ(ierr);
	if(checksweeps[0] != nbswp || checksweeps[1] != naswp)
		SETERRQ(PETSC_COMM_WORLD, PETSC_ERR_LIB, "Async sweeps not set properly!");
	return ierr;
}

void writeHeaderToFile(std::ofstream& outf, const int width)
{
	outf << '#' << std::setw(width) << "threads"
	     << std::setw(width) << "b&a-sweeps"
	     << std::setw(width+5) << "factor-speedup" << std::setw(width+5) << "apply-speedup"
	     << std::setw(width+5) << "total-speedup" << std::setw(width+5) << "total-deviate"
	     << std::setw(width) << "cpu-time" << std::setw(width+6) << "max-lin-iters"
	     << std::setw(width+4) << "avg-lin-iters"
	     << std::setw(width/2+1) << "conv?"
	     << "\n#---\n";
}

static void writeTimingToFile(std::ofstream& outf, const int w, const bool comment,
                              const TimingData& tdata, const RunParams rp,
                              const double factorspeedup, const double applyspeedup,
                              const double precspeedup, const double precdeviate,
                              const double preccputime)
{
	outf << (comment ? '#' : ' ') << std::setw(w) << rp.numthreads
	     << std::setw(w/2) << rp.nbswps << std::setw(w/2) << rp.naswps
	     << std::setw(w+5) << factorspeedup << std::setw(w+5) << applyspeedup
	     << std::setw(w+5) << precspeedup << std::setw(w+5) << precdeviate
	     << std::setw(w) << preccputime
	     << std::setw(w+6) << tdata.max_lin_iters << std::setw(w+4) << tdata.avg_lin_iters
	     << std::setw(w/2+1) << (tdata.converged ? 1 : 0)
	     << (comment ? "\n#---\n" : "\n") << std::flush;
}

static double std_deviation(const double *const vals, const double avg, const int N) {
	double deviate = 0;
	for(int j = 0; j < N; j++)
		deviate += (vals[j]-avg)*(vals[j]-avg);
	deviate = std::sqrt(deviate/(double)N);
	return deviate;
}

int run_one_test(const RunParams rp, const RunParams refp, const Mat A, const Vec b, Vec u,
                 TimingData& tdata, std::ofstream& outf)
{
	std::vector<double> prectimes(nruns);
	int ierr = 0;

	int rank = 1;
	MPI_Comm_rank(comm,&rank);

	int irun;
	for(irun = 0; irun < rp.nruns; irun++)
	{
		KSP ksp;
		ierr = KSPCreate(comm, &ksp);
		if(rp.ref)
			KSPSetOptionsPrefix(ksp, "ref_");
		ierr = KSPSetFromOptions(ksp); CHKERRQ(ierr);

		ierr = KSPSetOperators(ksp, A, A); CHKERRQ(ierr);

		Blasted_data_list bctx = newBlastedDataList();
		ierr = setup_blasted_stack(ksp, &bctx); CHKERRQ(ierr);

		ierr = VecSet(u, 0.0); CHKERRQ(ierr);

		ierr = KSPSolve(ksp, b, u); CHKERRQ(ierr);

		PetscInt refkspiters;
		ierr = KSPGetIterationNumber(ksp, &refkspiters);
		tdata.avg_lin_iters += refkspiters;
		if(tdata.max_lin_iters < refkspiters)
			tdata.max_lin_iters = refkspiters;

		if(rank==0 && rp.ref) {
			printf("Ref run: error = %.16f\n", errnormref);
		}

		KSPConvergedReason reason;
		ierr = KSPGetConvergedReason(ksp, &reason); CHKERRQ(ierr);
		if(reason <= 0) {
			tdata.converged = false;
			if(rank == 0)
				printf(" Solve failed with error %d", reason);
			break;
		}
		else {
			tdata.converged = true;
		}

		computeTotalTimes(&bctx);
		tdata.precsetup_walltime += bctx.factorwalltime;
		tdata.precapply_walltime += bctx.applywalltime;
		tdata.prec_cputime += bctx.factorcputime + bctx.applycputime;
		prectimes[irun] = bctx.factorwalltime + bctx.applywalltime;

		KSPDestroy(&ksp);
		destroyBlastedDataList(&bctx);
	}

	tdata.precsetup_walltime /= irun;
	tdata.precapply_walltime /= irun;
	tdata.prec_cputime /= irun;
	tdata.lin_iters = static_case<int>((float)tdata.lin_iters/(float)irun);

	const double prec_deviation = std_deviation(&prectimes[0],
	                                            tdata.precsetup_walltime+tdata.precapply_walltime,
	                                            irun);

	writeTimingToFile(outf, field_width, rp.ref, tdata, rp,
	                  refdata.precsetup_walltime/tdata.precsetup_walltime,
	                  refdata.precapply_walltime/tdata.precapply_walltime,
	                  (refdata.precsetup_walltime+refdata.precapply_walltime)
	                  /(tdata.precsetup_walltime+tdata.precapply_walltime),
	                  prec_deviation, tdata.prec_cputime);

	return ierr;
}
