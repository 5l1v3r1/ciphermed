/*
 * Copyright 2013-2015 Raluca Ada Popa
 *
 * This file is part of ciphermed.

 *  ciphermed is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  ciphermed is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with ciphermed.  If not, see <http://www.gnu.org/licenses/>. 2
 *
 */

#include "math/poly.hh"

#include <sstream>
#include <iostream>
#include <algorithm>
#include <util/util.hh>
#include <math/util.hh>

using namespace std;

static inline vector<mpz_class>
zerovec(size_t sz)
{
    vector<mpz_class> ret(sz);
    return ret;
}

// add polynomial P and Q
poly
operator+(const poly &P, const poly &Q)
{
    const size_t max_sz = max(P.size(),Q.size());
    const size_t min_sz = min(P.size(), Q.size());
    vector<mpz_class> res = zerovec(max_sz);
    for (size_t i = 0; i < min_sz; i++)
        res[i] = P[i] + Q[i];
    if (P.size() > min_sz) {
        for (size_t i = min_sz; i < P.size(); i++)
            res[i] = P[i];
    } else {
        for (size_t i = min_sz; i < Q.size(); i++)
            res[i] = Q[i];
    }
    return poly(move(res));
}

static inline ALWAYS_INLINE void
Realloc(mpz_class &m, size_t nlimbs)
{
    _mpz_realloc(m.get_mpz_t(), nlimbs);
}

// based off the general 1-pass KA:
// http://weimerskirch.org/papers/Weimerskirch_Karatsuba.pdf
poly
karatsuba2(const poly &p, const poly &q)
{
    const size_t n = max(p.deg() + 1, q.deg() + 1);
    if (unlikely(!n))
        return poly();
    vector<mpz_class> di(n);
    for (size_t i = 0; i < n; i++) {
        Realloc(di[i], 4);
        di[i] = p.element(i) * q.element(i);
    }
    vector<mpz_class> coeffs(2 * n - 1);

    // reserve space to avoid allocation overheads
    mpz_class tmp1, tmp2, tmp3, tmp4;
    Realloc(tmp1, 3);
    Realloc(tmp2, 3);
    Realloc(tmp3, 3);
    Realloc(tmp4, 4);
    for (auto &m : coeffs)
        Realloc(m, 4);

    for (size_t i = 1; i < 2 * n - 1; i++) {
        const bool odd = i % 2;
        const size_t upper = odd ? (i/2 + 1) : (i/2);
        mpz_class &ci = coeffs[i];
        for (size_t s = 0; s < upper; s++) {
            const size_t t = i - s;
            if (t >= n)
                continue;

            //ci += (p.element(s) + p.element(t)) * (q.element(s) + q.element(t));
            //ci -= (di[s] + di[t]);

            // use the scratch space
            tmp1 = p.element(s) + p.element(t);
            tmp2 = q.element(s) + q.element(t);
            tmp3 = di[s] + di[t];
            tmp4 = tmp1 * tmp2;

            ci += tmp4;
            ci -= tmp3;
        }
        if (!odd)
            ci += di[i / 2];
    }

    swap(coeffs[0], di[0]);
    swap(coeffs[2 * n - 2], di[n - 1]);
    return poly(move(coeffs));
}

//attempt at karatsuba poly multiplication
poly
karatsuba(const poly &P, const poly &Q)
{
	uint num_mult = 0;

//	assert_s(P.deg()==Q.deg(), "Trying to use karatsuba with polynomials of different degrees");

	uint deg = P.deg();
	uint n = deg+1;

	vector<mpz_class> di(n);
	for(uint i=0; i <= deg; i++){
		_mpz_realloc(di[i].get_mpz_t(), 4);
		di[i] = P[i]*Q[i];
	}

	num_mult += deg;

	uint limit = 2*n - 1;
	vector<mpz_class> dst = zerovec(limit);

        mpz_class tmp1("0",10), tmp2("0", 10), tmp3 ("0", 10), mult("0", 10);
        _mpz_realloc(tmp1.get_mpz_t(), 3);
        _mpz_realloc(tmp2.get_mpz_t(), 3);
        _mpz_realloc(tmp3.get_mpz_t(), 3);
        _mpz_realloc(mult.get_mpz_t(), 4);

        for(uint i = 0; i < limit; i++){
                _mpz_realloc(dst[i].get_mpz_t(), 4);
        }

	dst[0] = di[0];
	dst[limit-1] = di[deg];

	if(limit<2)
		return poly(move(dst));

	bool odd = true;
	uint t = 0, s = 0;
	for(uint i=1; i < n; i++){
		for(s = 0; s <= i/2; s++){
			t = i-s;
//			cerr<<"i: "<<i<<" s: "<<s<<" t: "<<t<<" n: "<<n<<endl;
			num_mult+=1;
			if(t>s && t < n){
//				cerr <<" ENTERED "<<endl;
//				(*dst)[i] += (P[s]+P[t])*(Q[s]+Q[t]) - di[s] - di[t];

                                tmp1 = P[s]+P[t];
                                tmp2 = Q[s]+Q[t];
                                tmp3 = di[s]+di[t];
                                mult = tmp1*tmp2;

                                dst[i] += mult;
                                dst[i] -= tmp3;
			}
		}
		if(odd) odd = false;
		else {
			dst[i] += di[i>>1];
			odd = true;
		}

	}
//multiples of 2 still enter loop extra time
	for(uint i=n; i <= limit-2; i++){
		for(s = i-n+1; s <= i/2; s++){
			t = i-s;
			num_mult+=1;
//			cerr<<"i: "<<i<<" s: "<<s<<" t: "<<t<<" n: "<<n<<endl;
			if(t>s && t < n){
//				cerr <<" ENTERED "<<endl;
//				(*dst)[i] += (P[s]+P[t])*(Q[s]+Q[t]) - di[s] - di[t];

                                tmp1 = P[s]+P[t];
                                tmp2 = Q[s]+Q[t];
                                tmp3 = di[s]+di[t];
                                mult = tmp1*tmp2;

                                dst[i] += mult;
                                dst[i] -= tmp3;

			}
		}
		if (odd) odd = false;
		else {
			dst[i] += di[i>>1];
			odd = true;
		}

	}

//	cerr << " karatsuba takes " << num_mult <<" mults." << endl;
	return poly(move(dst));
}

// multiply poly P and Q
// some efficient way of doing it?
poly
operator*(const poly & P, const poly & Q)
{
    //cerr << " txtbk mult takes " << (P.size() * Q.size()) <<" mults."<<endl;
    //return naive_multiply(P, Q);
    return karatsuba2(P, Q);
}

poly
operator*(const poly &p, const mpz_class &q)
{
    poly res = p;
    for (size_t i = 0; i < res.size(); i++)
        res[i] *= q;
    return res;
}

mpz_class
poly::eval(const mpz_class &x) const
{
    return naive_polyeval(*this, x);
}


poly
poly::modshift(const mpz_class &q) const
{
    assert(mpz_sgn(q.get_mpz_t()) == 1);
    mpz_class upper;
    if (mpz_odd_p(q.get_mpz_t()))
        upper = (q - 1) >> 1;
    else
        upper = q >> 1;
    vector<mpz_class> res = zerovec(size());
    for (uint i = 0; i < size(); i++) {
        // XXX: make more efficient
        mpz_class c = mpz_class_mod(coeffs_[i], q);
        if (c > upper)
            c -= q;
        res[i] = c;
    }

    return poly(move(res));
}

poly
poly::nearest_div(const mpz_class &q) const
{
    vector<mpz_class> copy(coeffs_);
    for (auto &c : copy)
        c = mpz_class_nearest_div(c, q);
    return poly(move(copy));
}

poly
operator%(const poly & P, const mpz_class & q)
{
    vector<mpz_class> res = zerovec(P.size());
    for (uint i = 0; i < P.size(); i++)
        res[i] = mpz_class_mod(P[i], q);
    return poly(move(res));
}

poly
operator-(const poly &P)
{
    poly ret = P;
    for (size_t i = 0; i < ret.size(); i++)
        ret[i] = -ret[i];
    return ret;
}

static void
subtract_monomial(poly &res, const mpz_class &c, uint delta)
{
    res[delta] = res[delta] - c;
    res.unsafe().resize(res.unsafe().size() - 1);
}

poly
modpoly(const poly & P, uint n)
{
    //we only divide by poly of the form x^n + 1
    // TODO: this could be optimized
    poly res = P;
    while (res.deg() >= n) {
        // the coeff for largest power
        mpz_class &c = res[res.size() - 1];
        subtract_monomial(res, c, res.deg() - n);
    }
    return res;
}

ostream&
operator<<(ostream & res, const poly & P)
{
    res << "{";
    for (uint i = 0; i < P.view().size(); i++) {
        res << P.view()[i];
        if (i != P.view().size()-1)
            res << ", ";
    }
    res << "}";
    return res;
}

bool
operator==(const poly &P, const poly &Q)
{
    const size_t pd = P.deg();
    const size_t qd = Q.deg();
    if (pd != qd)
        return false;
    for (size_t i = 0; i < pd; i++)
        if (P[i] != Q[i])
            return false;
    return true;
}

poly &
poly::operator+=(const poly &q)
{
    *this = (*this + q);
    return *this;
}

poly &
poly::operator*=(const poly &q)
{
    *this = (*this * q);
    return *this;
}

/* vim:set shiftwidth=4 ts=4 sts=4 et: */
