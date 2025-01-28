#include "../util/bytes.h"
#include "../util/crypto.h"
#include "../util/ssz.h"
#include "types_beacon.h"
#include <stdio.h>
#include <stdlib.h>

// the block hash proof is used as part of different other types since it contains all relevant
// proofs to validate the blockhash of the execution layer
const ssz_def_t BLOCK_HASH_PROOF[] = {
    SSZ_LIST("blockhash_proof", ssz_bytes32, 256),    // the merkle prooof from the executionPayload.blockhash down to the blockBodyRoot hash
    SSZ_CONTAINER("header", BEACON_BLOCK_HEADER),     // the header of the beacon block
    SSZ_BIT_VECTOR("sync_committee_bits", 512),       // the bits of the validators that signed the block
    SSZ_BYTE_VECTOR("sync_committee_signature", 96)}; // the signature of the sync committee

// the stateRoot proof is used as part of different other types since it contains all relevant
// proofs to validate the stateRoot of the execution layer
const ssz_def_t ETH_STATE_PROOF[] = {
    SSZ_LIST("state_proof", ssz_bytes32, 256),        // the merkle prooof from the executionPayload.state down to the blockBodyRoot hash
    SSZ_CONTAINER("header", BEACON_BLOCK_HEADER),     // the header of the beacon block
    SSZ_BIT_VECTOR("sync_committee_bits", 512),       // the bits of the validators that signed the block
    SSZ_BYTE_VECTOR("sync_committee_signature", 96)}; // the signature of the sync committee

const ssz_def_t ssz_bytes_1024 = SSZ_BYTES("Bytes", 1024);

// represents the storage proof of a key
const ssz_def_t ETH_STORAGE_PROOF[] = {
    SSZ_BYTES32("key"),                   // the key to be proven
    SSZ_LIST("proof", ssz_bytes_1024, 5), // Patricia merkle proof
    SSZ_BYTES32("value"),
};

const ssz_def_t ETH_STORAGE_PROOF_CONTAINER = SSZ_CONTAINER("StorageProof", ETH_STORAGE_PROOF);

// represents the account and storage values, including the Merkle proof, of the specified account.
const ssz_def_t ETH_ACCOUNT_PROOF[] = {
    SSZ_LIST("accountProof", ssz_bytes_1024, 256),              // Patricia merkle proof
    SSZ_ADDRESS("address"),                                     // the address of the account
    SSZ_BYTES32("balance"),                                     // the balance of the account
    SSZ_BYTES32("codeHash"),                                    // the code hash of the account
    SSZ_BYTES32("nonce"),                                       // the nonce of the account
    SSZ_BYTES32("storageHash"),                                 // the storage hash of the account
    SSZ_LIST("storageProof", ETH_STORAGE_PROOF_CONTAINER, 256), // the storage proofs of the selected
    SSZ_CONTAINER("state_proof", ETH_STATE_PROOF)};             // the state proof of the account

const ssz_def_t ETH_ACCOUNT_PROOF_CONTAINER   = SSZ_CONTAINER("AccountProof", ETH_ACCOUNT_PROOF);
const ssz_def_t LIGHT_CLIENT_UPDATE_CONTAINER = SSZ_CONTAINER("LightClientUpdate", LIGHT_CLIENT_UPDATE);

// A List of possible types of data matching the Proofs
const ssz_def_t C4_REQUEST_DATA_UNION[] = {
    SSZ_NONE,
    SSZ_BYTES32("blockhash")}; // the blochash  which is used for blockhash proof

// A List of possible types of proofs matching the Data
const ssz_def_t C4_REQUEST_PROOFS_UNION[] = {
    SSZ_NONE,
    SSZ_CONTAINER("BlockHashProof", BLOCK_HASH_PROOF),
    SSZ_CONTAINER("AccountProof", ETH_ACCOUNT_PROOF)}; // the blockhash proof used validating blockhashes

// A List of possible types of sync data used to update the sync state by verifying the transition from the last period to the required.
const ssz_def_t C4_REQUEST_SYNCDATA_UNION[] = {
    SSZ_NONE,
    SSZ_LIST("LightClientUpdate", LIGHT_CLIENT_UPDATE_CONTAINER, 512)}; // this light client update can be fetched directly from the beacon chain API

// the main container defining the incoming data processed by the verifier
const ssz_def_t C4_REQUEST[] = {
    SSZ_UNION("data", C4_REQUEST_DATA_UNION),           // the data to proof
    SSZ_UNION("proof", C4_REQUEST_PROOFS_UNION),        // the proof of the data
    SSZ_UNION("sync_data", C4_REQUEST_SYNCDATA_UNION)}; // the sync data containing proofs for the transition between the two periods

const ssz_def_t C4_REQUEST_CONTAINER = SSZ_CONTAINER("C4Request", C4_REQUEST);
