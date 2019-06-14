/** Checks correctness of the solution computed by BLASTed preconditioners.
 */

#undef NDEBUG

#include <petscksp.h>

#include <sys/time.h>
#include <ctime>
#include <cfloat>
#include <cassert>

#ifdef _OPENMP
#include <omp.h>
#endif

#include <blasted_petsc.h>

#include "poisson3d_fd.hpp"
#include "utils/cmdoptions.hpp"

#define PETSCOPTION_STR_LEN 30

PetscReal compute_error(const MPI_Comm comm, const CartMesh& m, const DM da,
		const Vec u, const Vec uexact) {
	PetscReal errnorm;
	Vec err;
	VecDuplicate(u, &err);
	VecCopy(u,err);
	VecAXPY(err, -1.0, uexact);
	errnorm = computeNorm(comm, &m, err, da);
	VecDestroy(&err);
	return errnorm;
}

int main(int argc, char* argv[])
{
	using namespace std;

	if(argc < 3) {
		printf("Please specify a control file and a Petsc options file.\n");
		return 0;
	}

	char help[] = "Solves 3D Poisson equation by finite differences.\
				   Arguments: (1) Control file (2) Petsc options file\n\n";

	char * confile = argv[1];
	PetscMPIInt size, rank;
	PetscErrorCode ierr = 0;
	int nruns;

	ierr = PetscInitialize(&argc, &argv, NULL, help); CHKERRQ(ierr);
	MPI_Comm comm = PETSC_COMM_WORLD;
	MPI_Comm_size(comm,&size);
	MPI_Comm_rank(comm,&rank);
	if(rank == 0)
		printf("Number of MPI ranks = %d.\n", size);

#ifdef _OPENMP
	const int nthreads = omp_get_max_threads();
	if(rank == 0)
		printf("Max OMP threads = %d\n", nthreads);
#endif

	// Read control file
	
	PetscInt npdim[NDIM];
	PetscReal rmax[NDIM], rmin[NDIM];
	char temp[50], gridtype[50];
	FILE* conf = fopen(confile, "r");
	int fstatus = 1;
	fstatus = fscanf(conf, "%s", temp);
	fstatus = fscanf(conf, "%s", gridtype);
	fstatus = fscanf(conf, "%s", temp);
	if(!fstatus) {
		std::printf("! Error reading control file!\n");
		std::abort();
	}
	for(int i = 0; i < NDIM; i++)
		fstatus = fscanf(conf, "%d", &npdim[i]);
	fstatus = fscanf(conf, "%s", temp);
	for(int i = 0; i < NDIM; i++)
		fstatus = fscanf(conf, "%lf", &rmin[i]);
	fstatus = fscanf(conf, "%s", temp);
	for(int i = 0; i < NDIM; i++)
		fstatus = fscanf(conf, "%lf", &rmax[i]);
	fstatus = fscanf(conf, "%s", temp); 
	fstatus = fscanf(conf, "%d", &nruns);
	fclose(conf);
	
	if(!fstatus) {
		std::printf("! Error reading control file!\n");
		std::abort();
	}

	if(rank == 0) {
		printf("Domain boundaries in each dimension:\n");
		for(int i = 0; i < NDIM; i++)
			printf("%f %f ", rmin[i], rmax[i]);
		printf("\n");
		printf("Number of runs: %d\n", nruns);
	}

	char testtype[PETSCOPTION_STR_LEN];
	PetscBool set = PETSC_FALSE;
	ierr = PetscOptionsGetString(NULL,NULL,"-test_type",testtype, PETSCOPTION_STR_LEN, &set);
	CHKERRQ(ierr);
	if(!set) {
		printf("Test type not set; testing issame.\n");
		strcpy(testtype,"issame");
	}

	// Get error check tolerance
	auto get_errtol = []() {
		PetscReal errtol = 0;
		PetscBool set = PETSC_FALSE;
		int ierr = PetscOptionsGetReal(NULL, NULL, "-error_tolerance", &errtol, &set);
		blasted::petsc_throw(ierr,"!! Could not get error tolerance!");
		if(!set) {
			printf("Error tolerance not set; using the default 2 times machine epsilon.");
			errtol = 2.0*DBL_EPSILON;
		}
		return errtol;
	};
	const PetscReal error_tol = get_errtol();

	//const PetscReal integer_error_tol = 1;

	set = PETSC_FALSE;
	PetscInt cmdnumruns;
	ierr = PetscOptionsGetInt(NULL,NULL,"-num_runs",&cmdnumruns,&set); CHKERRQ(ierr);
	if(set)
		nruns = cmdnumruns;

	//----------------------------------------------------------------------------------

	// set up Petsc variables
	DM da;                        ///< Distributed array context for the cart grid
	PetscInt ndofpernode = 1;
	PetscInt stencil_width = 1;
	DMBoundaryType bx = DM_BOUNDARY_GHOSTED;
	DMBoundaryType by = DM_BOUNDARY_GHOSTED;
	DMBoundaryType bz = DM_BOUNDARY_GHOSTED;
	DMDAStencilType stencil_type = DMDA_STENCIL_STAR;

	// grid structure - a copy of the mesh is stored by all processes as the mesh structure is very small
	CartMesh m;
	ierr = m.createMeshAndDMDA(comm, npdim, ndofpernode, stencil_width, bx, by, bz, stencil_type, 
			&da, rank);
	CHKERRQ(ierr);

	// generate grid
	if(!strcmp(gridtype, "chebyshev"))
		m.generateMesh_ChebyshevDistribution(rmin,rmax, rank);
	else
		m.generateMesh_UniformDistribution(rmin,rmax, rank);

	Vec u, uexact, b, err;
	Mat A;

	// create vectors and matrix according to the DMDA's structure
	
	ierr = DMCreateGlobalVector(da, &u); CHKERRQ(ierr);
	ierr = VecDuplicate(u, &b); CHKERRQ(ierr);
	VecDuplicate(u, &uexact);
	VecDuplicate(u, &err);
	VecSet(u, 0.0);
	ierr = DMCreateMatrix(da, &A); CHKERRQ(ierr);

	// compute values of LHS, RHS and exact soln
	
	ierr = computeRHS(&m, da, rank, b, uexact); CHKERRQ(ierr);
	ierr = computeLHS(&m, da, rank, A); CHKERRQ(ierr);

	// Assemble LHS

	ierr = MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);
	ierr = MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);

	KSP kspref; 

	// compute reference solution using a preconditioner from PETSc
	
	ierr = KSPCreate(comm, &kspref);
	KSPSetType(kspref, KSPRICHARDSON);
	KSPRichardsonSetScale(kspref, 1.0);
	KSPSetOptionsPrefix(kspref, "ref_");
	KSPSetFromOptions(kspref);
	
	ierr = KSPSetOperators(kspref, A, A); CHKERRQ(ierr);
	
	ierr = KSPSolve(kspref, b, u); CHKERRQ(ierr);

	KSPConvergedReason ref_ksp_reason;
	ierr = KSPGetConvergedReason(kspref, &ref_ksp_reason); CHKERRQ(ierr);
	assert(ref_ksp_reason > 0);

	PetscInt refkspiters;
	ierr = KSPGetIterationNumber(kspref, &refkspiters);
	PetscReal errnormref = compute_error(comm,m,da,u,uexact);

	if(rank==0) {
		printf("Ref run: error = %.16f\n", errnormref);
	}

	KSPDestroy(&kspref);

	// run the solve to be tested as many times as requested
	
	int avgkspiters = 0;
	PetscReal errnorm = 0;

	for(int irun = 0; irun < nruns; irun++)
	{
		if(rank == 0)
			printf("Run %d:\n", irun);
		KSP ksp;

		ierr = KSPCreate(comm, &ksp); CHKERRQ(ierr);
		ierr = KSPSetType(ksp, KSPRICHARDSON); CHKERRQ(ierr);
		ierr = KSPRichardsonSetScale(ksp, 1.0); CHKERRQ(ierr);

		// Options MUST be set before setting shell routines!
		ierr = KSPSetFromOptions(ksp); CHKERRQ(ierr);

		// Operators MUST be set before extracting sub KSPs!
		ierr = KSPSetOperators(ksp, A, A); CHKERRQ(ierr);

		// Create BLASTed data structure and setup the PC
		Blasted_data_list bctx = newBlastedDataList();
		ierr = setup_blasted_stack(ksp, &bctx); CHKERRQ(ierr);

		ierr = KSPSolve(ksp, b, u); CHKERRQ(ierr);

		// post-process
		int kspiters; PetscReal rnorm;
		ierr = KSPGetIterationNumber(ksp, &kspiters); CHKERRQ(ierr);
		avgkspiters += kspiters;

		KSPConvergedReason ksp_reason;
		ierr = KSPGetConvergedReason(ksp, &ksp_reason); CHKERRQ(ierr);
		assert(ksp_reason > 0);

		if(rank == 0) {
			ierr = KSPGetResidualNorm(ksp, &rnorm); CHKERRQ(ierr);
			printf(" KSP residual norm = %f\n", rnorm);
		}

		errnorm += compute_error(comm,m,da,u,uexact);
		if(rank == 0) {
			printf("Test run:\n");
			printf(" h and error: %f  %.16f\n", m.gh(), errnorm);
			printf(" log h and log error: %f  %f\n", log10(m.gh()), log10(errnorm));
		}

		ierr = KSPDestroy(&ksp); CHKERRQ(ierr);

		// rudimentary test for time-totaller
		computeTotalTimes(&bctx);
		assert(bctx.factorwalltime > DBL_EPSILON);
		assert(bctx.applywalltime > DBL_EPSILON);
		// looks like the problem is too small for the unix clock() to record it
		assert(bctx.factorcputime >= 0);
		assert(bctx.applycputime >= 0);

		destroyBlastedDataList(&bctx);
	}

	// if(rank == 0) {
	// 	printf("KSP Iters: Reference %d vs BLASTed %d.\n", refkspiters, avgkspiters/nruns);
	// 	printf("Difference in error norms: %g\n", fabs(errnormref-errnorm));
	// }
	// fflush(stdout);

	// assert(avgkspiters/nruns == refkspiters);
	// assert(std::fabs(errnorm-errnormref) < error_tol*DBL_EPSILON);

	printf("Error tolerance = %g.\n", error_tol);
	avgkspiters = avgkspiters/(double)nruns;
	if(rank == 0)
		printf("KSP Iters: Reference %d vs BLASTed %d.\n", refkspiters, avgkspiters);
	fflush(stdout);

	if(!strcmp(testtype, "compare_its") || !strcmp(testtype, "issame")) {
		assert(fabs((double)refkspiters - avgkspiters)/refkspiters <= error_tol);
	}
	else if(!strcmp(testtype, "upper_bound_its")) {
		assert(refkspiters > avgkspiters);
	}

	printf("Difference in error norm = %.16f.\n", std::fabs(errnorm-errnormref));
	printf("Relative difference in error norm = %e.\n", std::fabs(errnorm-errnormref)/errnormref);
	fflush(stdout);
	if(!strcmp(testtype,"compare_error") || !strcmp(testtype,"issame"))
		assert(std::fabs((errnorm/nruns-errnormref)/errnormref) < error_tol);

	VecDestroy(&u);
	VecDestroy(&uexact);
	VecDestroy(&b);
	VecDestroy(&err);
	MatDestroy(&A);
	DMDestroy(&da);
	PetscFinalize();

	return ierr;
}
		
// some unused snippets that might be useful at some point
	
	/*struct timeval time1, time2;
	gettimeofday(&time1, NULL);
	double initialwtime = (double)time1.tv_sec + (double)time1.tv_usec * 1.0e-6;
	double initialctime = (double)clock() / (double)CLOCKS_PER_SEC;*/

	/*gettimeofday(&time2, NULL);
	double finalwtime = (double)time2.tv_sec + (double)time2.tv_usec * 1.0e-6;
	double finalctime = (double)clock() / (double)CLOCKS_PER_SEC; */

/* For viewing the ILU factors computed by PETSc PCILU
#include <../src/mat/impls/aij/mpi/mpiaij.h>
#include <../src/ksp/pc/impls/factor/factor.h>
#include <../src/ksp/pc/impls/factor/ilu/ilu.h>*/

	/*if(precch == 'i') {	
		// view factors
		PC_ILU* ilu = (PC_ILU*)pc->data;
		//PC_Factor* pcfact = (PC_Factor*)pc->data;
		//Mat fact = pcfact->fact;
		Mat fact = ((PC_Factor*)ilu)->fact;
		printf("ILU0 factored matrix:\n");

		Mat_SeqAIJ* fseq = (Mat_SeqAIJ*)fact->data;
		for(int i = 0; i < fact->rmap->n; i++) {
			printf("Row %d: ", i);
			for(int j = fseq->i[i]; j < fseq->i[i+1]; j++)
				printf("(%d: %f) ", fseq->j[j], fseq->a[j]);
			printf("\n");
		}
	}*/

