#pragma once

#include <gmpxx.h>

namespace {

// computes |z|
inline mpz_class
mpz_class_abs(const mpz_class &z)
{
    mpz_class a;
    mpz_abs(a.get_mpz_t(), z.get_mpz_t());
    return a;
}

// computes nearest(p/q)
inline mpz_class
mpz_class_nearest_div(const mpz_class &p, const mpz_class &q)
{
    // XXX: slow
    if (mpz_divisible_p(p.get_mpz_t(), q.get_mpz_t()))
        return p/q;
    mpz_class ret;
    mpz_class pplusq = p + q;
    mpz_fdiv_q(ret.get_mpz_t(), pplusq.get_mpz_t(), q.get_mpz_t());
    return ret;
}

} // empty namespace

/* vim:set shiftwidth=4 ts=4 sts=4 et: */
