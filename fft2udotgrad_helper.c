#include <math.h>
#include <stdio.h>
#include "mex.h"

/* Helper function for fft2udotgrad */

/* $Id: fft2udotgrad_helper.c,v 1.5 2005/11/14 00:09:38 jeanluc Exp $ */

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  /* Dimension of grid. */
  int N;
  /* Counters */
  int k, m, iqx, iqy;
  /* Matlab sparse array. */
  mxArray *A;
  /* Pointers to internals of sparse matrix. */
  int *Air, *Ajc;
  double *Apr, *Api;
  /* ik is an array that translates from unsigned to signed vector
     components. */
  int *ik;
  /* Nonzero components of Fourier-transformed velocity field. */
  double *uxr, *uxi, *uyr, *uyi;
  /* Row and columns of nonzero elements. */
  double *nzx, *nzy;
  /* Imaginary part of ux and uy, if needed, otherwise leave at 0. */
  double uxim = 0, uyim = 0;
  /* Number of nonzero elements in ux, uy */
  int nnz;
  /* min/max mode numbers. */
  int kmin, kmax;

  if (nrhs < 6)
    {
      mexErrMsgTxt("6 input arguments required.");
    }

  N = (int)(mxGetScalar(prhs[0]));
  double L = mxGetScalar(prhs[5]);

  kmin = floor(-(N-1)/2);
  kmax = floor( (N-1)/2);
  ik = (int *)malloc(N*sizeof(int));
  for (m = 0; m <= kmax; ++m) ik[m] = m;
  for (m = -1; m >= kmin; --m) ik[N+m] = m;

  /* The row-dimension of the second argument, which is the number of
     nonzero elements of ux, uy. */
  nnz = mxGetM(prhs[1]);
  if (nnz != mxGetM(prhs[2]))
    {
      mexErrMsgTxt("ux and uy must have the same length.");
    }

  /* Pointers to the real part of the velocity data. */
  uxr = mxGetPr(prhs[1]);
  uyr = mxGetPr(prhs[2]);
  /* Only use the imaginary part if the matrix is complex. */
  if (mxIsComplex(prhs[1])) uxi = mxGetPi(prhs[1]);
  if (mxIsComplex(prhs[2])) uyi = mxGetPi(prhs[2]);

  /* The 4th and 5th arguments are integers, so only need the real
     part. Still get passed as doubles, though, as far as I can
     tell. */
  nzx = mxGetPr(prhs[3]);
  nzy = mxGetPr(prhs[4]);

  /* Matlab sparse matrix of dimension N^2 by N^2, with at most Acap
     nonzero elements. */
  /* Acap is computed by assuming there are no special
     cancellations. Do this by looping over the nonzero elements and
     computing how many values would be out of range.  This allows a
     fairly tight allocation of A. */
  int Acap = 0;
  for (m = 0; m < nnz; ++m)
    {
      /* Translate nonzero rows/columns to signed indices. */
      int mx = ik[(int)nzx[m]-1], my = ik[(int)nzy[m]-1];
      /* The number of nonzero values is determined by how many
	 elements would be out of range, which depends on the
	 magnitude of the index. */
      Acap += (N-abs(mx))*(N-abs(my));
    }
  A = mxCreateSparse(N*N,N*N,Acap,mxCOMPLEX);
  /* plhs[0] is a pointer to the first returned quantity. */
  plhs[0] = A;
  Air = mxGetIr(A);		/* row of sparse element */
  Ajc = mxGetJc(A);		/* number of nonzero elements in a column */
  Apr = mxGetPr(A);		/* real part of sparse data */
  Api = mxGetPi(A);		/* imaginary part of sparse data */

  /* Prefactor in rach term. */
  double fac = (2*M_PI/L)/(N*N);

  /* k counts the cumulative number of nonzero elements in row n-1.  It
     is nondecreasing. */
  k = 0;
  /* Loop over the columns Q = (qx,qy). Arrange such that the value of
     Q steps over each column, by looping over qx first. */
  for (iqy = 0; iqy < N; ++iqy)
    {
      for (iqx = 0; iqx < N; ++iqx)
	{
	  int Q = iqx + iqy*N;
	  /* Record the cumulative number of elements for the previous
	     column.  Don't ask me, that's just how it works, folks. */
	  Ajc[Q] = k;
	  /* Translate to signed indices. */
	  int qx = ik[iqx], qy = ik[iqy];
	  for (m = 0; m < nnz; ++m)
	    {
	      /* Translate nonzero rows/columns to signed indices and
		 add (qx,qy) to get the row of nonzero elements in
		 u.grad. In other words, the nonzero elements A_PQ are
		 those for which u_{P-Q} is a nonzero. */
	      int px = ik[(int)nzx[m]-1]+qx;
	      int py = ik[(int)nzy[m]-1]+qy;
	      /* If this doesn't result in an out-of-range index, then
		 compute the matrix element. */
	      if (px >= kmin && px <= kmax && py >= kmin && py <= kmax)
		{
		  /* Translate ipx and ipy to unsigned indices. */
		  int ipx = (px >= 0 ? px : N+px);
		  int ipy = (py >= 0 ? py : N+py);
		  int K = ipx + ipy*N;
		  /* Check if either ux or uy has an imaginary part. */
		  if (mxIsComplex(prhs[1])) uxim = uxi[m];
		  if (mxIsComplex(prhs[2])) uyim = uyi[m];
		  /* Ak(K,Q) = fac*i*(qx*ux(m) + qy*uy(m)) */
		  double Akr = -fac*(qx*uxim + qy*uyim);
		  double Aki =  fac*(qx*uxr[m] + qy*uyr[m]);
		  /* If the element wasn't zero, then record it in the
		     sparse matrix. */
		  if (Akr || Aki)
		    {
		      /* The row and real/imaginary parts of the element. */
		      Air[k] = K;
		      Apr[k] = Akr;
		      Api[k] = Aki;
		      /* Increase the nonzero element counter. */
		      ++k;
		      if (k > Acap)
			{
			  /* Oops! Capacity exceeded... this should
			     not happen. Reallocate or compute better. */
			  mexErrMsgTxt("Capacity exceeded.");
			}
		    }
		}
	    }
	}
    }
  /* The final element (one past the last column) records the total
     number of nonzero elements. */
  Ajc[N*N] = k;

  free(ik);

  return;
}