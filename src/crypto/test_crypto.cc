#include <assert.h>
#include <vector>
#include <crypto/elgamal.hh>
#include <NTL/ZZ.h>

#include<iostream>

using namespace std;
using namespace NTL;


static void
test_elgamal()
{
    auto sk = ElGamal_priv::keygen();
    ElGamal_priv pp(sk);
    
    auto pk = pp.pubkey();
    ElGamal p(pk);
    
    ZZ pt0 = RandomLen_ZZ(256);
    ZZ pt1 = RandomLen_ZZ(256);
    

    pair<ZZ,ZZ> ct0 = p.encrypt(pt0);
    pair<ZZ,ZZ> ct1 = p.encrypt(pt1);
    pair<ZZ,ZZ> prod = p.mult(ct0, ct1);
 
    assert(pp.decrypt(ct0) == pt0);
    assert(pp.decrypt(ct1) == pt1);
    assert(pp.decrypt(prod) == (pt0 * pt1));
}


int
main(int ac, char **av)
{
    test_elgamal();
    return 0;
}
