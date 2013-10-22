#pragma once

#define PORT 1990

/* Headers for the protocols */

const std::string GET_GM_PK = "GET GM PK";
const std::string END_GM_PK = "END GM PK";
const std::string GM_PK = "GM PK";


const std::string GET_PAILLIER_PK = "GET PAILLIER PK";
const std::string END_PAILLIER_PK = "END PAILLIER PK";
const std::string PAILLIER_PK = "PAILLIER PK";


const std::string START_LSIC = "START LSIC";
const std::string LSIC_SETUP = "LSIC SETUP";
const std::string LSIC_END = "LSIC END";
const std::string LSIC_PACKET = "LSIC PACKET";

const std::string DISCONNECT = "DISCONNECT";

const std::string DECRYPT_GM = "DECRYPT GM";
