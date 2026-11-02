#ifndef RISC201_ADDERS_HPP
#define RISC201_ADDERS_HPP

#include "LogicGates.hpp"
#include <tuple>
#include <string>

namespace risc201 {
namespace alu {

/**
 * 1. Ripple Carry Adder (RCA) - Sarangi Section 8.1.3
 * Chains n full adders bit-by-bit from LSB to MSB.
 * Time Complexity: O(n)
 */
inline std::tuple<Word, uint8_t, bool> rippleCarryAdd(Word a, Word b, uint8_t cin = 0) {
    Word sum = 0;
    uint8_t carry = cin;

    // Bit-by-bit full addition
    for (int i = 0; i < 32; ++i) {
        uint8_t a_bit = gates::get_bit(a, i);
        uint8_t b_bit = gates::get_bit(b, i);
        uint8_t s_bit = 0;
        uint8_t cout = 0;
        gates::full_adder(a_bit, b_bit, carry, s_bit, cout);
        gates::set_bit(sum, i, s_bit);
        carry = cout;
    }

    // Signed overflow detection (Theorem 2.3.4.1):
    // Overflow occurs if signs of A and B are same, but sign of sum is different
    uint8_t sign_a = gates::get_bit(a, 31);
    uint8_t sign_b = gates::get_bit(b, 31);
    uint8_t sign_s = gates::get_bit(sum, 31);
    bool overflow = (sign_a == sign_b) && (sign_s != sign_a);

    return {sum, carry, overflow};
}

/**
 * 2. Carry Select Adder (CSLA) - Sarangi Section 8.1.4
 * Divides the 32 bits into 4-bit blocks. Each block speculatively computes
 * two versions: one with cin=0, one with cin=1.
 * A 2:1 multiplexer selects the correct sum and carry-out when the true carry arrives.
 * Time Complexity: O(sqrt(n))
 */
inline std::tuple<Word, uint8_t, bool> carrySelectAdd(Word a, Word b, uint8_t cin = 0) {
    Word totalSum = 0;
    uint8_t currentCarry = cin;
    constexpr int BLOCK_SIZE = 4;
    constexpr int NUM_BLOCKS = 32 / BLOCK_SIZE;

    for (int blk = 0; blk < NUM_BLOCKS; ++blk) {
        int startBit = blk * BLOCK_SIZE;

        // Path 0: assuming carry-in is 0
        uint8_t sum0[BLOCK_SIZE]{0};
        uint8_t c0 = 0;
        for (int i = 0; i < BLOCK_SIZE; ++i) {
            uint8_t bit_a = gates::get_bit(a, startBit + i);
            uint8_t bit_b = gates::get_bit(b, startBit + i);
            gates::full_adder(bit_a, bit_b, c0, sum0[i], c0);
        }

        // Path 1: assuming carry-in is 1
        uint8_t sum1[BLOCK_SIZE]{0};
        uint8_t c1 = 1;
        for (int i = 0; i < BLOCK_SIZE; ++i) {
            uint8_t bit_a = gates::get_bit(a, startBit + i);
            uint8_t bit_b = gates::get_bit(b, startBit + i);
            gates::full_adder(bit_a, bit_b, c1, sum1[i], c1);
        }

        // Multiplexer selection based on current actual carry-in
        for (int i = 0; i < BLOCK_SIZE; ++i) {
            uint8_t selectedSum = gates::g_mux2(currentCarry, sum0[i], sum1[i]);
            gates::set_bit(totalSum, startBit + i, selectedSum);
        }
        currentCarry = gates::g_mux2(currentCarry, c0, c1);
    }

    uint8_t sign_a = gates::get_bit(a, 31);
    uint8_t sign_b = gates::get_bit(b, 31);
    uint8_t sign_s = gates::get_bit(totalSum, 31);
    bool overflow = (sign_a == sign_b) && (sign_s != sign_a);

    return {totalSum, currentCarry, overflow};
}

/**
 * 3. Carry Lookahead Adder (CLA) - Sarangi Section 8.1.5
 * Uses Generate (G) and Propagate (P) logic to compute carries in parallel prefix tree.
 * Gi = Ai * Bi
 * Pi = Ai XOR Bi
 * C_{i+1} = Gi + Pi * Ci
 * Time Complexity: O(log(n))
 */
inline std::tuple<Word, uint8_t, bool> carryLookaheadAdd(Word a, Word b, uint8_t cin = 0) {
    uint8_t g[32];
    uint8_t p[32];
    uint8_t c[33];
    c[0] = cin;

    // Generate and propagate functions for each bit pair (Equations 8.2 and 8.3)
    for (int i = 0; i < 32; ++i) {
        uint8_t a_bit = gates::get_bit(a, i);
        uint8_t b_bit = gates::get_bit(b, i);
        g[i] = gates::g_and(a_bit, b_bit);
        p[i] = gates::g_xor(a_bit, b_bit);
    }

    // 4-bit block lookahead expansion (Sarangi Section 8.1.5, Eq 8.8)
    // C1 = G0 + P0*C0
    // C2 = G1 + P1*G0 + P1*P0*C0
    // C3 = G2 + P2*G1 + P2*P1*G0 + P2*P1*P0*C0
    // C4 = G3 + P3*G2 + P3*P2*G1 + P3*P2*P1*G0 + P3*P2*P1*P0*C0
    for (int blk = 0; blk < 8; ++blk) {
        int base = blk * 4;
        uint8_t c_in_blk = c[base];

        // c[base + 1]
        c[base + 1] = gates::g_or(g[base], gates::g_and(p[base], c_in_blk));

        // c[base + 2]
        uint8_t t2_1 = gates::g_and(p[base + 1], g[base]);
        uint8_t t2_2 = gates::g_and(gates::g_and(p[base + 1], p[base]), c_in_blk);
        c[base + 2] = gates::g_or(g[base + 1], gates::g_or(t2_1, t2_2));

        // c[base + 3]
        uint8_t t3_1 = gates::g_and(p[base + 2], g[base + 1]);
        uint8_t t3_2 = gates::g_and(gates::g_and(p[base + 2], p[base + 1]), g[base]);
        uint8_t t3_3 = gates::g_and(gates::g_and(p[base + 2], p[base + 1]), gates::g_and(p[base], c_in_blk));
        c[base + 3] = gates::g_or(g[base + 2], gates::g_or(t3_1, gates::g_or(t3_2, t3_3)));

        // c[base + 4]
        uint8_t t4_1 = gates::g_and(p[base + 3], g[base + 2]);
        uint8_t t4_2 = gates::g_and(gates::g_and(p[base + 3], p[base + 2]), g[base + 1]);
        uint8_t t4_3 = gates::g_and(gates::g_and(p[base + 3], p[base + 2]), gates::g_and(p[base + 1], g[base]));
        uint8_t t4_4 = gates::g_and(gates::g_and(p[base + 3], p[base + 2]), gates::g_and(gates::g_and(p[base + 1], p[base]), c_in_blk));
        c[base + 4] = gates::g_or(g[base + 3], gates::g_or(t4_1, gates::g_or(t4_2, gates::g_or(t4_3, t4_4))));
    }

    // Compute sum bits: sum_i = p_i XOR c_i
    Word sum = 0;
    for (int i = 0; i < 32; ++i) {
        gates::set_bit(sum, i, gates::g_xor(p[i], c[i]));
    }

    uint8_t sign_a = gates::get_bit(a, 31);
    uint8_t sign_b = gates::get_bit(b, 31);
    uint8_t sign_s = gates::get_bit(sum, 31);
    bool overflow = (sign_a == sign_b) && (sign_s != sign_a);

    return {sum, c[32], overflow};
}

/**
 * 2's Complement Subtractor: A - B = A + (~B) + 1
 * Uses our Carry Lookahead Adder.
 */
inline std::tuple<Word, uint8_t, bool> subtract32(Word a, Word b) {
    Word notB = gates::bitwise_not(b);
    // Adding ~B with cin = 1 achieves 2's complement subtraction
    return carryLookaheadAdd(a, notB, 1);
}

} // namespace alu
} // namespace risc201

#endif // RISC201_ADDERS_HPP
