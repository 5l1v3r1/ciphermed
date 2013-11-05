#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <gmpxx.h>

#include <FHE.h>
#include <EncryptedArray.h>

#include <net/defs.hh>

#include <crypto/gm.hh>
#include <mpc/lsic.hh>
#include <mpc/private_comparison.hh>
#include <mpc/rev_enc_comparison.hh>
#include <mpc/enc_comparison.hh>
#include <mpc/linear_enc_argmax.hh>

#include <math/util_gmp_rand.h>

#include <net/net_utils.hh>
#include <net/message_io.hh>
#include <util/util.hh>
#include <util/fhe_util.hh>

#include <net/client.hh>

#include <protobuf/protobuf_conversion.hh>

using boost::asio::ip::tcp;

using namespace std;

Client::Client(boost::asio::io_service& io_service, gmp_randstate_t state,Key_dependencies_descriptor key_deps_desc, unsigned int keysize, unsigned int lambda)
: socket_(io_service),key_deps_desc_(key_deps_desc), gm_(NULL), paillier_(NULL), server_paillier_(NULL), server_gm_(NULL), fhe_context_(NULL), server_fhe_pk_(NULL), fhe_sk_(NULL), n_threads_(2), lambda_(lambda)
{
    gmp_randinit_set(rand_state_, state);
    
    init_needed_keys(keysize);
}

Client::~Client()
{
    if (server_fhe_pk_ != NULL) {
        delete server_fhe_pk_;
    }
    delete fhe_context_;
}


void Client::connect(boost::asio::io_service& io_service, const string& hostname)
{
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(hostname, to_string( PORT ));
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    boost::asio::connect(socket_, endpoint_iterator);
}

void Client::init_needed_keys(unsigned int keysize)
{
    if (key_deps_desc_.need_client_gm) {
        init_GM(keysize);
    }
    if (key_deps_desc_.need_client_paillier) {
        init_Paillier(keysize);
    }
    if (key_deps_desc_.need_client_fhe) {
        init_FHE_context();
        init_FHE_key();
    }
    
    if (key_deps_desc_.need_server_fhe) {
        init_FHE_context();
    }
}

void Client::init_GM(unsigned int keysize)
{
    if (gm_ != NULL) {
        return;
    }
    gm_ = new GM_priv(GM_priv::keygen(rand_state_,keysize),rand_state_);
}

void Client::init_Paillier(unsigned int keysize)
{
    if (paillier_ != NULL) {
        return;
    }
    paillier_ = new Paillier_priv_fast(Paillier_priv_fast::keygen(rand_state_,keysize), rand_state_);

}

void Client::init_FHE_context()
{
    if (fhe_context_) {
        return;
    }
    // generate a context. This one should be consisten with the server's one
    // i.e. m, p, r must be the same
    
    fhe_context_ = create_FHEContext(FHE_p,FHE_r,FHE_d,FHE_c,FHE_L,FHE_s,FHE_k,FHE_m);
    // we suppose d > 0
    fhe_G_ = makeIrredPoly(FHE_p, FHE_d);
}
void Client::init_FHE_key()
{
    if (fhe_sk_) {
        return;
    }
    
    fhe_sk_ = new FHESecKey(*fhe_context_);
    fhe_sk_->GenSecKey(FHE_w); // A Hamming-weight-w secret key
}


void Client::get_server_pk_gm()
{
    if (server_gm_) {
        return;
    }

    Protobuf::GM_PK pk = readMessageFromSocket<Protobuf::GM_PK>(socket_);
    cout << "Received GM PK" << endl;
    server_gm_ = create_from_pk_message(pk,rand_state_);
}


void Client::get_server_pk_paillier()
{
    if (server_paillier_) {
        return;
    }

    Protobuf::Paillier_PK pk = readMessageFromSocket<Protobuf::Paillier_PK>(socket_);
    cout << "Received Paillier PK" << endl;
    server_paillier_ = create_from_pk_message(pk,rand_state_);
}

void Client::get_server_pk_fhe()
{
    if (server_fhe_pk_) {
        return;
    }
    
    Protobuf::FHE_PK pk = readMessageFromSocket<Protobuf::FHE_PK>(socket_);
    cout << "Received FHE PK" << endl;
    server_fhe_pk_ = create_from_pk_message(pk,*fhe_context_);
}

void Client::send_gm_pk()
{
    assert(gm_!=NULL);
    Protobuf::GM_PK pk_message = get_pk_message(gm_);
    sendMessageToSocket<Protobuf::GM_PK>(socket_,pk_message);
}

void Client::send_paillier_pk()
{
    assert(paillier_ != NULL);
    Protobuf::Paillier_PK pk_message = get_pk_message(paillier_);
    sendMessageToSocket<Protobuf::Paillier_PK>(socket_,pk_message);
}

void Client::send_fhe_pk()
{
    const FHEPubKey& publicKey = *fhe_sk_; // cast so we only send the public informations
    
    Protobuf::FHE_PK pk_message = get_pk_message(publicKey);
    
    sendMessageToSocket<Protobuf::FHE_PK>(socket_,pk_message);
    
}

void Client::exchange_keys()
{
    if (key_deps_desc_.need_server_gm) {
        get_server_pk_gm();
    }
    if (key_deps_desc_.need_server_paillier) {
        get_server_pk_paillier();
    }
    if (key_deps_desc_.need_server_fhe) {
        get_server_pk_fhe();
    }

    
    if (key_deps_desc_.need_client_gm) {
        send_gm_pk();
    }
    if (key_deps_desc_.need_client_paillier) {
        send_paillier_pk();
    }
    if (key_deps_desc_.need_client_fhe) {
        send_fhe_pk();
    }
}

mpz_class Client::run_comparison_protocol_A(Comparison_protocol_A *comparator)
{
    if(typeid(*comparator) == typeid(LSIC_A)) {
        run_lsic_A(reinterpret_cast<LSIC_A*>(comparator));
    }else if(typeid(*comparator) == typeid(Compare_A)){
        run_priv_compare_A(reinterpret_cast<Compare_A*>(comparator));
    }
    
    return comparator->output();
}

mpz_class Client::run_lsic_A(LSIC_A *lsic)
{
    
    LSIC_Packet_A a_packet;
    LSIC_Packet_B b_packet;
    Protobuf::LSIC_A_Message a_message;
    Protobuf::LSIC_B_Message b_message;
    
    bool state;
    
    
    boost::asio::streambuf out_buff;
    std::ostream output_stream(&out_buff);
    string line;
    
    
    // response-request
    for (; ; ) {
        b_message = readMessageFromSocket<Protobuf::LSIC_B_Message>(socket_);
        b_packet = convert_from_message(b_message);

        state = lsic->answerRound(b_packet,&a_packet);
                
        if (state) {
            return lsic->output();
        }
        
        a_message = convert_to_message(a_packet);
        sendMessageToSocket(socket_, a_message);
    }
 }

mpz_class Client::run_priv_compare_A(Compare_A *comparator)
{
    boost::asio::streambuf out_buff;
    std::ostream output_stream(&out_buff);
    string line;
    std::istream input_stream(&input_buf_);
    
    vector<mpz_class> c_b(comparator->bit_length());
    
    // first get encrypted bits
    
    Protobuf::BigIntArray c_b_message = readMessageFromSocket<Protobuf::BigIntArray>(socket_);
    c_b = convert_from_message(c_b_message);
    
    vector<mpz_class> c_w = comparator->compute_w(c_b);
    vector<mpz_class> c_sums = comparator->compute_sums(c_w);
    vector<mpz_class> c = comparator->compute_c(c_b,c_sums);
    vector<mpz_class> c_rand = comparator->rerandomize_parallel(c,n_threads_);
    
    // we have to suffle
    random_shuffle(c_rand.begin(),c_rand.end(),[this](int n){ return gmp_urandomm_ui(rand_state_,n); });
    
    // send the result
    
    Protobuf::BigIntArray c_rand_message = convert_to_message(c_rand);
    sendMessageToSocket(socket_, c_rand_message);
    
    // wait for the encrypted result
    mpz_class c_t_prime;

    Protobuf::BigInt c_t_prime_message = readMessageFromSocket<Protobuf::BigInt>(socket_);
    c_t_prime = convert_from_message(c_t_prime_message);

    comparator->unblind(c_t_prime);

    return comparator->output();

    
}



void Client::run_comparison_protocol_B(Comparison_protocol_B *comparator)
{
    if(typeid(*comparator) == typeid(LSIC_B)) {
        run_lsic_B(reinterpret_cast<LSIC_B*>(comparator));
    }else if(typeid(*comparator) == typeid(Compare_B)){
        run_priv_compare_B(reinterpret_cast<Compare_B*>(comparator));
    }
}

void Client::run_lsic_B(LSIC_B *lsic)
{
    cout << "Start LSIC B" << endl;
    boost::asio::streambuf output_buf;
    std::ostream output_stream(&output_buf);
    std::string line;
    
    LSIC_Packet_A a_packet;
    LSIC_Packet_B b_packet = lsic->setupRound();
    Protobuf::LSIC_A_Message a_message;
    Protobuf::LSIC_B_Message b_message;
    
    b_message = convert_to_message(b_packet);
    sendMessageToSocket(socket_, b_message);
    
    cout << "LSIC setup sent" << endl;
    
    // wait for packets
    
    for (;b_packet.index < lsic->bitLength()-1; ) {
        a_message = readMessageFromSocket<Protobuf::LSIC_A_Message>(socket_);
        a_packet = convert_from_message(a_message);
        
        b_packet = lsic->answerRound(a_packet);
        
        b_message = convert_to_message(b_packet);
        sendMessageToSocket(socket_, b_message);
    }
    
    cout << "LSIC B Done" << endl;
}

void Client::run_priv_compare_B(Compare_B *comparator)
{
//    boost::asio::streambuf output_buf;
//    std::ostream output_stream(&output_buf);
//    std::istream input_stream(&input_buf_);
//    std::string line;
    
    vector<mpz_class> c(comparator->bit_length());
    
    
    // send the encrypted bits
    Protobuf::BigIntArray c_b_message = convert_to_message(comparator->encrypt_bits());
    sendMessageToSocket(socket_, c_b_message);
    
    // wait for the answer from the client
    Protobuf::BigIntArray c_message = readMessageFromSocket<Protobuf::BigIntArray>(socket_);
    c = convert_from_message(c_message);
    
    
    //    input_stream >> c;
    
    mpz_class c_t_prime = comparator->search_zero(c);
    
    // send the blinded result
    Protobuf::BigInt c_t_prime_message = convert_to_message(c_t_prime);
    sendMessageToSocket(socket_, c_t_prime_message);
    
}


// we suppose that the client already has the server's public key for Paillier
void Client::run_rev_enc_comparison_owner(const mpz_class &a, const mpz_class &b, size_t l)
{
    assert(has_paillier_pk());
    assert(has_gm_pk());
    
//    LSIC_A comparator(0,l,*server_gm_);
    Compare_A comparator(0,l,*server_paillier_,*server_gm_,rand_state_);

    Rev_EncCompare_Owner owner(a,b,l,*server_paillier_,&comparator,rand_state_);
    run_rev_enc_comparison_owner(owner);
}

void Client::run_rev_enc_comparison_owner(Rev_EncCompare_Owner &owner)
{
    assert(has_paillier_pk());
    assert(has_gm_pk());
 
    size_t l = owner.bit_length();
    mpz_class c_z(owner.setup(lambda_));

    
    boost::asio::streambuf out_buff;
    std::ostream output_stream(&out_buff);
    string line;
    
    Protobuf::Enc_Compare_Setup_Message setup_message = convert_to_message(c_z,l);
    sendMessageToSocket(socket_, setup_message);

    // the server does some computation, we just have to run the lsic
    
    run_comparison_protocol_A(owner.comparator());
    
    Protobuf::BigInt c_z_l_message = readMessageFromSocket<Protobuf::BigInt>(socket_);
    mpz_class c_z_l = convert_from_message(c_z_l_message);


    mpz_class c_t = owner.concludeProtocol(c_z_l);
    
    // send the last message to the server
    Protobuf::BigInt c_t_message = convert_to_message(c_t);
    sendMessageToSocket(socket_, c_t_message);
}


bool Client::run_rev_enc_comparison_helper(const size_t &l)
{
    assert(has_paillier_pk());
    assert(has_gm_pk());

    //    LSIC_B comparator(0,l,server_->gm());
    Compare_B comparator(0,l,*paillier_,*gm_);
    
    Rev_EncCompare_Helper helper(l,*paillier_,&comparator);
    return run_rev_enc_comparison_helper(helper);
}

bool Client::run_rev_enc_comparison_helper(Rev_EncCompare_Helper &helper)
{
    boost::asio::streambuf output_buf;
    std::ostream output_stream(&output_buf);
    std::istream input_stream(&input_buf_);
    std::string line;
    
    
    // setup the helper if necessary
    if (!helper.is_set_up()) {
        cout << "Have to setup" << endl;
        Protobuf::Enc_Compare_Setup_Message setup_message = readMessageFromSocket<Protobuf::Enc_Compare_Setup_Message>(socket_);
        if (setup_message.has_bit_length()) {
            helper.set_bit_length(setup_message.bit_length());
        }
        mpz_class c_z = convert_from_message(setup_message);
        
        helper.setup(c_z);
    }
    
    // now, we need to run the comparison protocol
    run_comparison_protocol_B(helper.comparator());
    
    
    mpz_class c_z_l(helper.get_c_z_l());
    
    Protobuf::BigInt c_z_l_message = convert_to_message(c_z_l);
    sendMessageToSocket(socket_, c_z_l_message);
    
    // wait for the answer of the owner
    Protobuf::BigInt c_t_message = readMessageFromSocket<Protobuf::BigInt>(socket_);
    mpz_class c_t = convert_from_message(c_t_message);
    helper.decryptResult(c_t);
    return helper.output();
    
}

// we suppose that the client already has the server's public key for Paillier
bool Client::run_enc_comparison_owner(const mpz_class &a, const mpz_class &b, size_t l)
{
    assert(has_paillier_pk());
    assert(has_gm_pk());
    assert(gm_!=NULL);

    LSIC_B lsic(0,l,*gm_);
    EncCompare_Owner owner(a,b,l,*server_paillier_,&lsic,rand_state_);
    return run_enc_comparison_owner(owner);
}

bool Client::run_enc_comparison_owner(EncCompare_Owner &owner)
{
    assert(has_paillier_pk());
    
    // now run the protocol itself
    size_t l = owner.bit_length();
    mpz_class c_z(owner.setup(lambda_));

    Protobuf::Enc_Compare_Setup_Message setup_message = convert_to_message(c_z,l);
    sendMessageToSocket(socket_, setup_message);

    // the server does some computation, we just have to run the lsic
    
    run_comparison_protocol_B(owner.comparator());

    mpz_class c_r_l(owner.get_c_r_l());
    Protobuf::BigInt c_r_l_message = convert_to_message(c_r_l);
    sendMessageToSocket(socket_, c_r_l_message);

    // wait for the answer of the owner
    Protobuf::BigInt c_t_message = readMessageFromSocket<Protobuf::BigInt>(socket_);
    mpz_class c_t = convert_from_message(c_t_message);
    
    owner.decryptResult(c_t);
    return owner.output();
}

void Client::run_enc_comparison_helper(const size_t &l)
{
    assert(server_gm_ != NULL);
#warning WE MUST BE ABLE TO CHOOSE COMPARISON PROTOCOL
    LSIC_A lsic(0,l,*server_gm_);
    EncCompare_Helper helper(l,*paillier_,&lsic);
    run_enc_comparison_helper(helper);
}

void Client::run_enc_comparison_helper(EncCompare_Helper &helper)
{
    boost::asio::streambuf output_buf;
    std::ostream output_stream(&output_buf);
    std::istream input_stream(&input_buf_);
    std::string line;
    
    // setup the helper if necessary
    if (!helper.is_set_up()) {
        Protobuf::Enc_Compare_Setup_Message setup_message = readMessageFromSocket<Protobuf::Enc_Compare_Setup_Message>(socket_);
        if (setup_message.has_bit_length()) {
            helper.set_bit_length(setup_message.bit_length());
        }
        mpz_class c_z = convert_from_message(setup_message);
        
        helper.setup(c_z);
    }
    
    // now, we need to run the comparison protocol
    run_comparison_protocol_A(helper.comparator());
    
    Protobuf::BigInt c_r_l_message = readMessageFromSocket<Protobuf::BigInt>(socket_);
    mpz_class c_r_l = convert_from_message(c_r_l_message);
    
    mpz_class c_t = helper.concludeProtocol(c_r_l);
    
    // send the last message to the server
    Protobuf::BigInt c_t_message = convert_to_message(c_t);
    sendMessageToSocket(socket_, c_t_message);
}



size_t Client::run_linear_enc_argmax(Linear_EncArgmax_Owner &owner)
{
    assert(has_paillier_pk());
    assert(has_gm_pk());

    size_t k = owner.elements_number();
    size_t nbits = owner.bit_length();
    //    auto party_a_creator = [gm_ptr,p_ptr,nbits,randstate_ptr](){ return new Compare_A(0,nbits,*p_ptr,*gm_ptr,*randstate_ptr); };

    cout << "Number of elements " << k << endl;
    for (size_t i = 0; i < (k-1); i++) {
//        cout << "Round " << i << endl;
        Compare_A comparator(0,nbits,*server_paillier_,*server_gm_,rand_state_);
//        LSIC_A comparator(0,nbits,*server_gm_);
        
        Rev_EncCompare_Owner rev_enc_owner = owner.create_current_round_rev_enc_compare_owner(&comparator);
        
        run_rev_enc_comparison_owner(rev_enc_owner);

        mpz_class randomized_enc_max, randomized_value;
        owner.next_round(randomized_enc_max, randomized_value);
        
        // send the randomizations to the server
        sendIntToSocket(socket_,randomized_enc_max);
        sendIntToSocket(socket_,randomized_value);
        
        // get the server's response
        mpz_class new_enc_max, x, y;
        new_enc_max = readIntFromSocket(socket_);
        x = readIntFromSocket(socket_);
        y = readIntFromSocket(socket_);
        
        owner.update_enc_max(new_enc_max, x, y);
    }
    
    mpz_class permuted_argmax;
    permuted_argmax = readIntFromSocket(socket_);
    
    owner.unpermuteResult(permuted_argmax.get_ui());
    
    return owner.output();
}

EncCompare_Owner Client::create_enc_comparator_owner(size_t bit_size, bool use_lsic)
{
    assert(has_paillier_pk());
    assert(gm_!=NULL);

    Comparison_protocol_B *comparator;
    
    if (use_lsic) {
        comparator = new LSIC_B(0,bit_size,*gm_);
    }else{
        assert(paillier_ != NULL);
        comparator = new Compare_B(0,bit_size,*paillier_,*gm_);
    }

    return EncCompare_Owner(0,0,bit_size,*server_paillier_,comparator,rand_state_);
}

EncCompare_Helper Client::create_enc_comparator_helper(size_t bit_size, bool use_lsic)
{
    assert(paillier_ != NULL);

    Comparison_protocol_A *comparator;
    
    if (use_lsic) {
        comparator = new LSIC_A(0,bit_size,*server_gm_);
    }else{
        comparator = new Compare_A(0,bit_size,*server_paillier_,*server_gm_,rand_state_);
    }
    
    return EncCompare_Helper(bit_size,*paillier_,comparator);
}

Rev_EncCompare_Owner Client::create_rev_enc_comparator_owner(size_t bit_size, bool use_lsic)
{
    assert(has_paillier_pk());
    assert(has_gm_pk());

    Comparison_protocol_A *comparator;
    
    if (use_lsic) {
        comparator = new LSIC_A(0,bit_size,*server_gm_);
    }else{
        comparator = new Compare_A(0,bit_size,*server_paillier_,*server_gm_,rand_state_);
    }
    
    return Rev_EncCompare_Owner(0,0,bit_size,*server_paillier_,comparator,rand_state_);
}

Rev_EncCompare_Helper Client::create_rev_enc_comparator_helper(size_t bit_size, bool use_lsic)
{
    assert(gm_!=NULL);
    assert(paillier_ != NULL);

    Comparison_protocol_B *comparator;
    
    if (use_lsic) {
        comparator = new LSIC_B(0,bit_size,*gm_);
    }else{
        comparator = new Compare_B(0,bit_size,*paillier_,*gm_);
    }
    
    return Rev_EncCompare_Helper(bit_size,*paillier_,comparator);
}