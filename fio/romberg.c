#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
const char *romberg_module_id = "$Id: romberg.c,v 1.5 2008/09/18 13:14:30 dpotter Exp $";

#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#define FLOAT_MAXVAL HUGE
#define MAXLEV 13

/*
 ** Romberg integrator for an open interval.
 */

double
dRombergO(void *CTX,double (*func)(void *, double),double a,double b,
	  double eps) {
    double tllnew;
    double tll;
    double tlk[MAXLEV+1];
    int n = 1;
    int nsamples = 1;

    tlk[0] = tllnew = (b-a)*(*func)(CTX, 0.5*(b+a));
    if (a == b) return tllnew;
    tll = FLOAT_MAXVAL;

    eps*=0.5;

    while ((fabs((tllnew-tll)/(tllnew+tll)) > eps) && (n < MAXLEV)) {
	/*
	 * midpoint rule.
	 */
	double deltax;
	double tlktmp;
	int i;

	nsamples *= 3;
	deltax = (b-a)/nsamples;
	tlktmp = tlk[0];
	tlk[0] = tlk[0]/3.0;

	for (i=0;i<nsamples/3;i++) {
	    tlk[0] += deltax*(*func)(CTX,a + (3*i + 0.5)*deltax);
	    tlk[0] += deltax*(*func)(CTX,a + (3*i + 2.5)*deltax);
	    }

	/*
	 * Romberg extrapolation.
	 */

	for (i=0;i<n;i++) {
	    double tlknew = (pow(9.0, i+1.)*tlk[i] - tlktmp)
			    /(pow(9.0, i+1.) - 1.0);

	    tlktmp = tlk[i+1];
	    tlk[i+1] = tlknew;
	    }
	tll = tllnew;
	tllnew = tlk[n];
	n++;
	}

    assert((fabs((tllnew-tll)/(tllnew+tll)) < eps));

    return tllnew;
    }
