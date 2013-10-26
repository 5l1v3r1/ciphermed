#pragma once

#include <vector>
#include <gmpxx.h>
#include <crypto/paillier.hh>
#include <crypto/gm.hh>

#include <mpc/comparison_protocol.hh>

class Compare_A : public Comparison_protocol_A {
public:
    Compare_A(const mpz_class &x, const size_t &l, Paillier &paillier, GM &gm, gmp_randstate_t state);
    
    void set_value(const mpz_class &x) { a_ = x; };

    std::vector<mpz_class> compute_w(const std::vector<mpz_class> &c_b);
    std::vector<mpz_class> compute_sums(const std::vector<mpz_class> &c_w);
    std::vector<mpz_class> compute_c(const std::vector<mpz_class> &c_a,const std::vector<mpz_class> &c_sums);
    std::vector<mpz_class> rerandomize(const std::vector<mpz_class> &c);
    void unblind(const mpz_class &t_prime);
    
    GM gm() const { return gm_; }
    mpz_class output() const { return res_; }
protected:
    mpz_class a_;
    long s_;
    size_t bit_length_; // bit length of the numbers to compare
    Paillier paillier_;
    GM gm_;
    
    mpz_class res_;
    mpz_class paillier_one_;
};

class Compare_B : public Comparison_protocol_B {
public:
    Compare_B(const mpz_class &y, const size_t &l, Paillier_priv &paillier, GM_priv &gm);
    
    virtual void set_value(const mpz_class &x) { b_ = x; };

    std::vector<mpz_class> encrypt_bits();
    std::vector<mpz_class> encrypt_bits_fast();
    std::vector<mpz_class> encrypt_bits_fast_precompute();
    mpz_class search_zero(const std::vector<mpz_class> &c);
    
    GM_priv gm() const { return gm_; };

protected:
    mpz_class b_;
    size_t bit_length_; // bit length of the numbers to compare
    Paillier_priv paillier_;
    GM_priv gm_;
};



void runProtocol(Compare_A &party_a, Compare_B &party_b, gmp_randstate_t state);
inline void runProtocol(Compare_A *party_a, Compare_B *party_b, gmp_randstate_t state)
{
    runProtocol(*party_a,*party_b, state);
}