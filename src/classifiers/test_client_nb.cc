#include <classifiers/nb_classifier.hh>

#include <util/util.hh>
#include <util/benchmarks.hh>

static vector<long> gen_nursery_query()
{
    vector<long> query(8);
    
    query[0] = 1 + (rand() %3);
    query[1] = 1 + (rand() %5);
    query[2] = 1 + (rand() %4);
    query[3] = 1 + (rand() %4);
    query[4] = 1 + (rand() %3);
    query[5] = 1 + (rand() %2);
    query[6] = 1 + (rand() %3);
    query[7] = 1 + (rand() %3);
    
    return query;
}

static vector<long> gen_ecg_query()
{
    vector<long> query(5);
    long modulo = 1; // 2^44
    modulo <<= 44;
    
    query[0] = (1-2*(rand()%2)) * (rand() % modulo);
    query[1] = (1-2*(rand()%2)) * (rand() % modulo);
    query[2] = (1-2*(rand()%2)) * (rand() % modulo);
    query[3] = (1-2*(rand()%2)) * (rand() % modulo);
    query[4] = (1-2*(rand()%2)) * (rand() % modulo);
    
    return query;

}

static void test_nb_classifier_client(const string &hostname)
{
    try
    {
#ifdef BENCHMARK
        cout << "BENCHMARK flag set" << endl;
        BENCHMARK_INIT
#endif
        
   
        boost::asio::io_service io_service;
        
        gmp_randstate_t randstate;
        gmp_randinit_default(randstate);
        gmp_randseed_ui(randstate,time(NULL));


        vector<mpz_class> query;
        unsigned int n_nodes;
        
        Naive_Bayes_Classifier_Client client(io_service, randstate,1024,100, query,64);
        
        client.connect(io_service, hostname);
        
        client.run();
        
//        client.disconnect();
        
    }
    catch (std::exception& e)
    {
        std::cout << "Exception: " << e.what() << std::endl;
    }
    
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: client <host>" << std::endl;
        return 1;
    }
    string hostname(argv[1]);
    srand(time(NULL));

    test_nb_classifier_client(hostname);
    
    return 0;
}