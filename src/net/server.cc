#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <gmpxx.h>
#include <thread>

#include <net/defs.hh>

#include <crypto/paillier.hh>
#include <mpc/lsic.hh>

#include <net/server.hh>
#include <net/net_utils.hh>

using boost::asio::ip::tcp;

using namespace std;

Server::Server(gmp_randstate_t state, unsigned int nbits_p, unsigned int abits_p, unsigned int nbits_gm, unsigned int lambda)
: paillier_(Paillier_priv::keygen(state,nbits_p,abits_p),state), gm_(GM_priv::keygen(state,nbits_gm),state), lambda_(lambda), n_clients_(0)
{
    gmp_randinit_set(rand_state_, state);
//    cout << "SK GM\np = " << gm_.privkey()[0] << "\nq = " << gm_.privkey()[1] << endl;
//    cout << "PK Paillier\nn" << paillier_.pubkey()[0] << "\ng" << paillier_.pubkey()[1] << endl;
}

void Server::run()
{
    try
    {
        boost::asio::io_service io_service;
        
        tcp::endpoint endpoint(tcp::v4(), PORT);
        tcp::acceptor acceptor(io_service, endpoint);
        
        for (;;)
        {
            tcp::socket *socket = new tcp::socket(io_service);
            acceptor.accept(*socket);
            
            Server_session *c = new Server_session(this, rand_state_, n_clients_++, socket);
            
            cout << "Start new connexion: " << c->id() << endl;
            thread t (&Server_session::run_session,c);
            t.detach();
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}



Server_session::Server_session(Server *server, gmp_randstate_t state, unsigned int id, tcp::socket *socket)
: server_(server), socket_(socket), id_(id)
{
    gmp_randinit_set(rand_state_, state);
}

void Server_session::run_session()
{
    cout << id_ << ": Start session" << endl;
    
    // main loop to catch requests
    bool should_exit = false;
    try {
            for (;!should_exit; ) {
        
        // wait for a complete request
        boost::asio::read_until(*socket_, input_buf_, "\r\n");
        
        std::istream input_stream(&input_buf_);
        std::string line;
        
    //    std::string s( (std::istreambuf_iterator<char>( input_stream )),
    //                  (std::istreambuf_iterator<char>()) );
    //    cout << s << endl;

        // parse the input
        do {
            getline(input_stream,line);
//            cout << line;
            if (line == "") {
                continue;
            }

            if (line == "GET PK PAILLIER") {
                send_paillier_pk();
            }else if(line == "GET PK GM") {
                send_gm_pk();
            }else if(line == "START LSIC") {
                mpz_class b(20);
                run_lsic(b,5);
            }else if(line == "DECRYPT GM") {
                mpz_class c(5);
                getline(input_stream,line);
                c.set_str(line,10);
                decrypt_gm(c);
            }else if(line == "DISCONNECT"){
                should_exit = true;
                break;
            }
        } while (!input_stream.eof());
    }
    cout << id_ << ": Disconnected" << endl;

        
    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }

    // we are done, delete ourself
    delete this;
}

void Server_session::send_paillier_pk()
{
    auto pk = server_->paillier_pk();
    boost::asio::streambuf buff;
    std::ostream buff_stream(&buff);
    
    cout << id_ << ": Send Paillier PK" << endl;
    buff_stream << "PAILLIER PK\n";
    buff_stream << pk[0] << "\n" << pk[1] << "\n";
    
    buff_stream << "END PAILLIER PK\n";
    boost::asio::write(*socket_, buff);
}

void Server_session::send_gm_pk()
{
    auto pk = server_->gm_pk();
    boost::asio::streambuf buff;
    std::ostream buff_stream(&buff);
    
    cout << id_ << ": Send GM PK" << endl;
    buff_stream << "GM PK\n";
    buff_stream << pk[0] << "\n" << pk[1] << "\n";
    
    buff_stream << "END GM PK\n";
    boost::asio::write(*socket_, buff);
}

void Server_session::run_lsic(const mpz_class &b,size_t l)
{
    cout << id_ << ": Start LSIC" << endl;
    boost::asio::streambuf output_buf;
    std::ostream output_stream(&output_buf);
    std::string line;

    LSIC_B lsic(b,l, server_->gm_sk(),rand_state_);
    
    LSIC_Packet_A a_packet;
    LSIC_Packet_B b_packet = lsic.setupRound();

    output_stream << "LSIC SETUP\n";
    output_stream << b_packet;
    output_stream << "\r\n";
    
    boost::asio::write(*socket_, output_buf);
    
//    cout << "LSIC setup sent" << endl;
    
    // wait for packets
    
    for (; ; ) {
        boost::asio::read_until(*socket_, input_buf_, "\r\n");
        std::istream input_stream(&input_buf_);

//        cout << "Received something" << endl;
        // parse the input
        do {
            getline(input_stream,line);
//            cout << line;
            if (line == "") {
                continue;
            }
            
            if (line == "LSIC END") {
                cout << id_ << ": LSIC finished" << endl;
                return;
            }else if(line == "LSIC PACKET") {
//                cout << "New packet" << endl;
                input_stream >> a_packet;

                b_packet = lsic.answerRound(a_packet);
                
                boost::asio::streambuf output_buf;
                std::ostream output_stream(&output_buf);
                
                output_stream << "LSIC PACKET\n";
                output_stream << b_packet;
                output_stream << "\r\n";
                
                boost::asio::write(*socket_, output_buf);
                
//                cout << "Sent packet " << b_packet.index << endl;
            }
        } while (!input_stream.eof());
    }
    
}

void Server_session::decrypt_gm(const mpz_class &c)
{
    bool b = (server_->gm()).decrypt(c);
    cout << id_ << ": Decryption result = " << b << endl;
}

int main()
{
    gmp_randstate_t randstate;
    gmp_randinit_default(randstate);
    gmp_randseed_ui(randstate,time(NULL));
    

    cout << "Init server" << endl;
    Server server(randstate,1024,256,1024,100);
    
    cout << "Start server" << endl;
    server.run();
    
    return 0;
}