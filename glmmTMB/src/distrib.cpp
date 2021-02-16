//#include <TMB.hpp>
#include <RcppEigen.h>
#include <tmb_core.hpp>
#include "distrib.h"

// additional distributions etc. for glmmTMB

// FIXME: check proper syntax for including 
namespace glmmtmb{
	
  template<class Type>
  Type dbetabinom(Type y, Type a, Type b, Type n, int give_log)
  {
    /*
      Wikipedia:
      f(k|n,\alpha,\beta) =
      \frac{\Gamma(n+1)}{\Gamma(k+1)\Gamma(n-k+1)}
      \frac{\Gamma(k+\alpha)\Gamma(n-k+\beta)}{\Gamma(n+\alpha+\beta)}
      \frac{\Gamma(\alpha+\beta)}{\Gamma(\alpha)\Gamma(\beta)}
    */
    Type logres =
      lgamma(n + 1) - lgamma(y + 1)     - lgamma(n - y + 1) +
      lgamma(y + a) + lgamma(n - y + b) - lgamma(n + a + b) +
      lgamma(a + b) - lgamma(a)         - lgamma(b) ;
    if(!give_log) return exp(logres);
    else return logres;
  }
	
  template<class Type>
  Type dgenpois(Type y, Type theta, Type lambda, int give_log)
  {
    /*
      f(y|\theta,\lambda) =
      \frac{\theta(theta+\lambda y)^{y-1}e^{-\theta-\lambda y}}{y \!}
    */
    Type logres =
      log(theta) + (y - 1) * log(theta + lambda * y) -
      theta - lambda * y - lgamma(y + Type(1));
    if(!give_log) return exp(logres);
    else return logres;
  }

   // from C. Geyer aster package
   // Simulate from truncated poisson
   double rtruncpois(int k, double mu)
   {
    int m;
    double mdoub;

    if (mu <= 0.0)
	    throw std::range_error("non-positive mu in k-truncated-poisson simulator\n");
    if (k < 0)
	    throw std::range_error("negative k in k-truncated-poisson simulator\n");

    mdoub = k + 1 - mu;
    if (mdoub < 0.0)
        mdoub = 0.0;
    m = mdoub;
    if (m < mdoub)
        m = m + 1;
    /* since mu > 0.0 we have 0.0 <= mdoub < k + 1 hence 0 <= m <= k + 1 */

    for (;;) {
        double x = rpois(mu) + m;
        if (m > 0) {
            double a = 1.0;
            int j;
            double u = unif_rand();
            for (j = 0; j < m; ++j)
                a *= (k + 1 - j) / (x - j);
            if (u < a && x > k)
                return x;
        } else {
            if (x > k)
                return x;
        }
    }
   } // rtruncpois

  /* Simulate from generalized poisson distribution */
  template<class Type>
  Type rgenpois(Type theta, Type lambda) {
    // Copied from R function HMMpa::rgenpois
    Type ans = Type(0);
    Type random_number = runif(Type(0), Type(1));
    Type kum = dgenpois(Type(0), theta, lambda);
    while (random_number > kum) {
      ans = ans + Type(1);
      kum += dgenpois(ans, theta, lambda);
    }
    return ans;
  }
	
  /* Simulate from zero-truncated generalized poisson distribution */
  template<class Type>
  Type rtruncated_genpois(Type theta, Type lambda) {
    int nloop = 10000;
    int counter = 0;
    Type ans = rgenpois(theta, lambda);
    while(ans < Type(1) && counter < nloop) {
      ans = rgenpois(theta, lambda);
      counter++;
    }
    if(ans < 1.) warning("Zeros in simulation of zero-truncated data. Possibly due to low estimated mean.");
    return ans;
  }

  /* y(x) = logit_invcloglog(x) := log( exp(exp(x)) - 1 ) = logspace_sub( exp(x), 0 )

     y'(x) = exp(x) + exp(x-y) = exp( logspace_add(x, x-y) )

   */
  TMB_ATOMIC_VECTOR_FUNCTION(
                             // ATOMIC_NAME
                             logit_invcloglog
                             ,
                             // OUTPUT_DIM
                             1,
                             // ATOMIC_DOUBLE
                             ty[0] = Rf_logspace_sub(exp(tx[0]), 0.);
                             ,
                             // ATOMIC_REVERSE
                             px[0] = exp( logspace_add(tx[0], tx[0]-ty[0]) ) * py[0];
                             )
  template<class Type>
  Type logit_invcloglog(Type x) {
    CppAD::vector<Type> tx(1);
    tx[0] = x;
    return logit_invcloglog(tx)[0];
  }

  /* y(x) = logit_pnorm(x) := logit( pnorm(x) ) =
     pnorm(x, lower.tail=TRUE,  log.p=TRUE) -
     pnorm(x, lower.tail=FALSE, log.p=TRUE)

     y'(x) = dnorm(x) * ( (1+exp(y)) + (1+exp(-y)) )

  */
  double logit_pnorm(double x) {
    double log_p_lower, log_p_upper;
    Rf_pnorm_both(x, &log_p_lower, &log_p_upper, 2 /* both tails */, 1 /* log_p */);
    return log_p_lower - log_p_upper;
  }
  TMB_ATOMIC_VECTOR_FUNCTION(
                             // ATOMIC_NAME
                             logit_pnorm
                             ,
                             // OUTPUT_DIM
                             1,
                             // ATOMIC_DOUBLE
                             ty[0] = logit_pnorm(tx[0])
                             ,
                             // ATOMIC_REVERSE
                             Type zero = 0;
                             Type tmp1 = logspace_add(zero, ty[0]);
                             Type tmp2 = logspace_add(zero, -ty[0]);
                             Type tmp3 = logspace_add(tmp1, tmp2);
                             Type tmp4 = dnorm(tx[0], Type(0), Type(1), true) + tmp3;
                             px[0] = exp( tmp4 ) * py[0];
                             )
  template<class Type>
  Type logit_pnorm(Type x) {
    CppAD::vector<Type> tx(1);
    tx[0] = x;
    return logit_pnorm(tx)[0];
  }

  /* Calculate variance in compois family using

     V(X) = (logZ)''(loglambda)

  */
  double compois_calc_var(double mean, double nu){
    using atomic::compois_utils::calc_loglambda;
    using atomic::compois_utils::calc_logZ;
    double loglambda = calc_loglambda(log(mean), nu);
    typedef atomic::tiny_ad::variable<2, 1, double> ADdouble;
    ADdouble loglambda_ (loglambda, 0);
    ADdouble ans = calc_logZ<ADdouble>(loglambda_, nu);
    return ans.getDeriv()[0];
  }

  /* Simulate from zero-truncated Conway-Maxwell-Poisson distribution */
  template<class Type>
  Type rtruncated_compois2(Type mean, Type nu) {
    int nloop = 10000;
    int counter = 0;
    Type ans = rcompois2(mean, nu);
    while(ans < 1. && counter < nloop) {
      ans = rcompois2(mean, nu);
      counter++;
    }
    if(ans < 1.) warning("Zeros in simulation of zero-truncated data. Possibly due to low estimated mean.");
    return ans;
  }

  /* Simulate from tweedie distribution */
  template<class Type>
  Type rtweedie(Type mu, Type phi, Type p) {
    // Copied from R function tweedie::rtweedie
    Type lambda = pow(mu, 2. - p) / (phi * (2. - p));
    Type alpha  = (2. - p) / (1. - p);
    Type gam = phi * (p - 1.) * pow(mu, p - 1.);
    int N = (int) asDouble(rpois(lambda));
    Type ans = rgamma(N, -alpha /* shape */, gam /* scale */).sum();
    return ans;
  }
} // namespace glmmtmb

/* Interface to compois variance */
extern "C" {
  SEXP compois_calc_var(SEXP mean, SEXP nu) {
    if (LENGTH(mean) != LENGTH(nu))
      error("'mean' and 'nu' must be vectors of same length.");
    SEXP ans = PROTECT(allocVector(REALSXP, LENGTH(mean)));
    for(int i=0; i<LENGTH(mean); i++)
      REAL(ans)[i] = glmmtmb::compois_calc_var(REAL(mean)[i], REAL(nu)[i]);
    UNPROTECT(1);
    return ans;
  }
}

	
