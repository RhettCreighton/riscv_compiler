/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ethereum_rlp_decode.c - Ethereum RLP (Recursive Length Prefix) decoder circuit
 * 
 * RLP is Ethereum's encoding method for serializing data structures.
 * This circuit decodes RLP-encoded data and validates its structure.
 * 
 * RLP encoding rules:
 * - Single byte [0x00, 0x7f]: encoded as itself
 * - String 0-55 bytes: 0x80 + length, followed by string
 * - String >55 bytes: 0xb7 + length_of_length, length, followed by string
 * - List 0-55 bytes total: 0xc0 + length, followed by concatenated RLP items
 * - List >55 bytes total: 0xf7 + length_of_length, length, followed by items
 */

#include "../include/zkvm.h"
#include <string.h>

// RLP item types
typedef enum {
    RLP_STRING = 0,
    RLP_LIST = 1,
    RLP_INVALID = 2
} rlp_type_t;

// Decoded RLP item metadata
typedef struct {
    rlp_type_t type;
    uint32_t data_offset;    // Offset to actual data (after length prefix)
    uint32_t data_length;    // Length of the data
    uint32_t total_length;   // Total length including prefix
    uint32_t is_valid;       // 1 if valid, 0 otherwise
} rlp_item_t;

// Helper: Decode length from multi-byte encoding
// Returns the decoded length value
ZKVM_INLINE static uint32_t decode_multibyte_length(const uint8_t* data, uint32_t length_bytes) GATES(2000) {
    uint32_t length = ZERO;
    
    // Big-endian decode
    for (uint32_t i = 0; i < length_bytes; i++) {
        length = (length << 8) | data[i];
    }
    
    return length;
}

// Decode a single RLP item starting at data[offset]
// Returns metadata about the decoded item
ZKVM_INLINE static rlp_item_t decode_rlp_item(const uint8_t* data, uint32_t offset, uint32_t max_length) GATES(5000) {
    rlp_item_t item;
    item.is_valid = ONE;
    
    // Check bounds
    if (offset >= max_length) {
        item.is_valid = ZERO;
        item.type = RLP_INVALID;
        return item;
    }
    
    uint8_t prefix = data[offset];
    
    // Case 1: Single byte [0x00, 0x7f]
    if (prefix <= 0x7f) {
        item.type = RLP_STRING;
        item.data_offset = offset;
        item.data_length = 1;
        item.total_length = 1;
        return item;
    }
    
    // Case 2: String 0-55 bytes [0x80, 0xb7]
    if (prefix >= 0x80 && prefix <= 0xb7) {
        uint32_t str_len = prefix - 0x80;
        item.type = RLP_STRING;
        item.data_offset = offset + 1;
        item.data_length = str_len;
        item.total_length = 1 + str_len;
        
        // Validate bounds
        if (offset + item.total_length > max_length) {
            item.is_valid = ZERO;
        }
        return item;
    }
    
    // Case 3: String >55 bytes [0xb8, 0xbf]
    if (prefix >= 0xb8 && prefix <= 0xbf) {
        uint32_t length_bytes = prefix - 0xb7;
        
        // Check if we have enough bytes for length
        if (offset + 1 + length_bytes > max_length) {
            item.is_valid = ZERO;
            item.type = RLP_INVALID;
            return item;
        }
        
        uint32_t str_len = decode_multibyte_length(&data[offset + 1], length_bytes);
        item.type = RLP_STRING;
        item.data_offset = offset + 1 + length_bytes;
        item.data_length = str_len;
        item.total_length = 1 + length_bytes + str_len;
        
        // Validate bounds and canonical encoding
        if (offset + item.total_length > max_length || str_len <= 55) {
            item.is_valid = ZERO;
        }
        return item;
    }
    
    // Case 4: List 0-55 bytes [0xc0, 0xf7]
    if (prefix >= 0xc0 && prefix <= 0xf7) {
        uint32_t list_len = prefix - 0xc0;
        item.type = RLP_LIST;
        item.data_offset = offset + 1;
        item.data_length = list_len;
        item.total_length = 1 + list_len;
        
        // Validate bounds
        if (offset + item.total_length > max_length) {
            item.is_valid = ZERO;
        }
        return item;
    }
    
    // Case 5: List >55 bytes [0xf8, 0xff]
    if (prefix >= 0xf8) {
        uint32_t length_bytes = prefix - 0xf7;
        
        // Check if we have enough bytes for length
        if (offset + 1 + length_bytes > max_length) {
            item.is_valid = ZERO;
            item.type = RLP_INVALID;
            return item;
        }
        
        uint32_t list_len = decode_multibyte_length(&data[offset + 1], length_bytes);
        item.type = RLP_LIST;
        item.data_offset = offset + 1 + length_bytes;
        item.data_length = list_len;
        item.total_length = 1 + length_bytes + list_len;
        
        // Validate bounds and canonical encoding
        if (offset + item.total_length > max_length || list_len <= 55) {
            item.is_valid = ZERO;
        }
        return item;
    }
    
    // Should never reach here
    item.is_valid = ZERO;
    item.type = RLP_INVALID;
    return item;
}

// Verify that an Ethereum block header is properly RLP encoded
// Block header should be a list containing exactly 15 items
uint32_t verify_ethereum_block_header_rlp(const uint8_t* rlp_data, uint32_t data_length) {
    // First, decode the outer list
    rlp_item_t header = decode_rlp_item(rlp_data, 0, data_length);
    
    // Must be a valid list
    if (!header.is_valid || header.type != RLP_LIST) {
        return ZERO;
    }
    
    // Must match the total data length
    if (header.total_length != data_length) {
        return ZERO;
    }
    
    // Now decode each field in the header
    uint32_t offset = header.data_offset;
    uint32_t fields_decoded = 0;
    
    // Ethereum block header fields (15 total):
    // 1. parentHash (32 bytes)
    // 2. ommersHash (32 bytes)
    // 3. beneficiary (20 bytes)
    // 4. stateRoot (32 bytes)
    // 5. transactionsRoot (32 bytes)
    // 6. receiptsRoot (32 bytes)
    // 7. logsBloom (256 bytes)
    // 8. difficulty (variable)
    // 9. number (variable)
    // 10. gasLimit (variable)
    // 11. gasUsed (variable)
    // 12. timestamp (variable)
    // 13. extraData (0-32 bytes)
    // 14. mixHash (32 bytes)
    // 15. nonce (8 bytes)
    
    // Expected field lengths (0 means variable)
    uint32_t expected_lengths[15] = {32, 32, 20, 32, 32, 32, 256, 0, 0, 0, 0, 0, 0, 32, 8};
    
    while (offset < header.data_offset + header.data_length && fields_decoded < 15) {
        rlp_item_t field = decode_rlp_item(rlp_data, offset, data_length);
        
        if (!field.is_valid) {
            return ZERO;
        }
        
        // Check field type (all should be strings)
        if (field.type != RLP_STRING) {
            return ZERO;
        }
        
        // Check expected length if specified
        if (expected_lengths[fields_decoded] > 0 && 
            field.data_length != expected_lengths[fields_decoded]) {
            return ZERO;
        }
        
        // Special validation for extraData (field 12): max 32 bytes
        if (fields_decoded == 12 && field.data_length > 32) {
            return ZERO;
        }
        
        offset += field.total_length;
        fields_decoded++;
    }
    
    // Must have exactly 15 fields
    if (fields_decoded != 15) {
        return ZERO;
    }
    
    // Must have consumed exactly the list length
    if (offset != header.data_offset + header.data_length) {
        return ZERO;
    }
    
    return ONE;
}

// Example: Verify a simplified Ethereum block header
int main() {
    // Example RLP-encoded Ethereum block header (simplified)
    // This is a minimal valid header with mostly empty/zero fields
    uint8_t rlp_header[] = {
        0xf9, 0x02, 0x1a,  // List prefix: 0xf9 means list >55 bytes, 0x021a = 538 bytes
        
        // parentHash (32 bytes)
        0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // ommersHash (32 bytes)
        0xa0, 0x1d, 0xcc, 0x4d, 0xe8, 0xde, 0xc7, 0x5d, 0x7a,
        0xab, 0x85, 0xb5, 0x67, 0xb6, 0xcc, 0xd4, 0x1a,
        0xd3, 0x12, 0x45, 0x1b, 0x94, 0x8a, 0x74, 0x13,
        0xf0, 0xa1, 0x42, 0xfd, 0x40, 0xd4, 0x93, 0x47,
        
        // beneficiary (20 bytes)
        0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        
        // Remaining fields would follow...
        // For brevity, using a shorter example
    };
    
    // Verify the RLP encoding
    uint32_t is_valid = verify_ethereum_block_header_rlp(
        rlp_header, 
        sizeof(rlp_header)
    );
    
    // Output result
    zkvm_output(&is_valid, 1);
    
    // Expected gate count:
    // - RLP decoding: ~5K gates per item × 15 items = 75K gates
    // - Validation logic: ~10K gates
    // - Total: ~85K gates
    
    zkvm_checkpoint("RLP verification complete");
    
    return is_valid;
}