#pragma once

#include <proto_src/proto_headers.hh>

#include <gmpxx.h>

#include <crypto/gm.hh>
#include <crypto/paillier.hh>
#include <mpc/lsic.hh>

mpz_class convert_from_message(const Protobuf::BigInt &m);
Protobuf::BigInt convert_to_message(const mpz_class &v);

std::vector<mpz_class> convert_from_message(const Protobuf::BigIntArray &m);
Protobuf::BigIntArray convert_to_message(const std::vector<mpz_class> &v);

LSIC_Packet_A convert_from_message(const Protobuf::LSIC_A_Message &m);
LSIC_Packet_B convert_from_message(const Protobuf::LSIC_B_Message &m);
Protobuf::LSIC_A_Message convert_to_message(const LSIC_Packet_A &p);
Protobuf::LSIC_B_Message convert_to_message(const LSIC_Packet_B &p);

GM* create_from_pk_message(const Protobuf::GM_PK &m_pk, gmp_randstate_t state);
Paillier* create_from_pk_message(const Protobuf::Paillier_PK &m_pk, gmp_randstate_t state);

Protobuf::GM_PK get_pk_message(const GM *gm);
Protobuf::Paillier_PK get_pk_message(const Paillier *paillier);