#include <NTL/ZZ.h>
#include <vector>
#include <array>
#include <utility>
#include <pthread.h>


#include <mpc/millionaire.hh>
#include <crypto/paillier.hh>



class SimpleClassifier_Client {
public:
    SimpleClassifier_Client();
    
    std::vector<NTL::ZZ> paillierPubKey() const {return paillier_.pubkey(); };
    std::vector<NTL::ZZ> millionairePubParam() const {return millionaire_.pubparams(); };
    
    std::vector<NTL::ZZ> encryptVector(const std::vector<NTL::ZZ> &vec);
    std::vector< std::array<std::pair<NTL::ZZ,NTL::ZZ>,2> > decryptAndStartMillionaire(const NTL::ZZ &c);
    bool compareResult(const std::vector< std::pair<NTL::ZZ,NTL::ZZ> > &c) const;
    
protected:
    Paillier_priv paillier_;
    Millionaire_Alice millionaire_;
};

class SimpleClassifier_Server {
public:
    SimpleClassifier_Server(const std::vector<long> v);
    ~SimpleClassifier_Server();
    
    std::vector<std::pair<size_t,NTL::ZZ> > randomizedDotProduct(std::vector<NTL::ZZ> &vec, size_t nQueries, const std::vector<NTL::ZZ> &pk_paillier);
    std::vector<std::pair<size_t,NTL::ZZ> > randomizedDotProduct_smallError(std::vector<NTL::ZZ> &vec, size_t nQueries, const std::vector<NTL::ZZ> &pk_paillier, const NTL::ZZ &errorBound);

    std::vector< std::pair<NTL::ZZ,NTL::ZZ> > compareRandomness(const std::vector< std::array<std::pair<NTL::ZZ,NTL::ZZ>,2> > &T,const std::vector<NTL::ZZ> &mil_pubparam, size_t i_query);
    
protected:
    const std::vector<long> model_;
    const size_t m_length_;
    size_t queries_count_;
    std::vector<NTL::ZZ> randomness_;
    pthread_mutex_t mutex_queries_;
};

