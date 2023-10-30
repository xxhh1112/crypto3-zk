//---------------------------------------------------------------------------//
// Copyright (c) 2021 Mikhail Komarov <nemo@nil.foundation>
// Copyright (c) 2021 Nikita Kaskov <nbering@nil.foundation>
// Copyright (c) 2021 Ilias Khairullin <ilias@nil.foundation>
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//---------------------------------------------------------------------------//

#ifndef CRYPTO3_ZK_TRANSCRIPT_FIAT_SHAMIR_HEURISTIC_HPP
#define CRYPTO3_ZK_TRANSCRIPT_FIAT_SHAMIR_HEURISTIC_HPP

#include <nil/marshalling/algorithms/pack.hpp>
#include <nil/crypto3/marshalling/algebra/types/field_element.hpp>

#include <nil/crypto3/hash/type_traits.hpp>
#include <nil/crypto3/hash/algorithm/hash.hpp>
#include <nil/crypto3/hash/sha2.hpp>
#include <nil/crypto3/hash/keccak.hpp>

#include <nil/crypto3/algebra/curves/pallas.hpp>
#include <nil/crypto3/algebra/fields/arithmetic_params/pallas.hpp>
#include <nil/crypto3/hash/poseidon.hpp>

#include <nil/crypto3/hash/detail/poseidon/kimchi_constants.hpp>
#include <nil/crypto3/hash/detail/poseidon/original_constants.hpp>
#include <nil/crypto3/hash/detail/poseidon/poseidon_sponge.hpp>
#include <nil/crypto3/hash/detail/poseidon/poseidon_policy.hpp>
#include <nil/crypto3/hash/detail/poseidon/poseidon_permutation.hpp>
#include <nil/crypto3/hash/detail/poseidon/poseidon_sponge.hpp>
#include <nil/crypto3/hash/detail/block_stream_processor.hpp>

#include <nil/crypto3/multiprecision/cpp_int.hpp>
namespace nil {
    namespace crypto3 {
        namespace zk {
            namespace transcript {

                /*!
                 * @brief Fiat–Shamir heuristic.
                 * @tparam Hash Hash function, which serves as a non-interactive random oracle.
                 * @tparam TManifest Fiat-Shamir Heuristic Manifest in the following form:
                 *
                 * template<typename ...>
                 * struct fiat_shamir_heuristic_manifest {
                 *
                 *     struct transcript_manifest {
                 *         std::size_t gammas_amount = 5;
                 *       public:
                 *         enum challenges_ids{
                 *             alpha,
                 *             beta,
                 *             gamma = 10,
                 *             delta = gamma + gammas_amount,
                 *             epsilon
                 *         }
                 *
                 *     }
                 * };
                 */
                template<typename ChallengesType, typename Hash>
                class fiat_shamir_heuristic_accumulative {

                    accumulator_set<Hash> acc;

                public:
                    typedef Hash hash_type;
                    typedef ChallengesType challenges_type;

                    fiat_shamir_heuristic_accumulative() : acc() {
                    }

                    template<typename TAny>
                    void operator()(TAny data) {
                        nil::marshalling::status_type status;
                        typename hash_type::construction::type::block_type byte_data =
                            nil::marshalling::pack(data, status);
                        acc(byte_data);
                    }

                    template<typename ChallengesType::challenges_ids ChallengeId, typename FieldType>
                    typename FieldType::value_type challenge() {
                        // acc(ChallengeId);
                        typename hash_type::digest_type hash_res = accumulators::extract::hash<Hash>(acc);

                        return FieldType::value_type::one();
                    }

                    template<typename ChallengesType::challenges_ids ChallengeId, std::size_t Index, typename FieldType>
                    typename FieldType::value_type challenge() {
                        // acc(ChallengeId + Index);
                        typename hash_type::digest_type hash_res = accumulators::extract::hash<Hash>(acc);

                        return FieldType::value_type::one();
                    }

                    template<typename ChallengesType::challenges_ids ChallengeId, std::size_t ChallengesAmount,
                             typename FieldType>
                    std::array<typename FieldType::value_type, ChallengesAmount> challenges() {

                        std::array<typename hash_type::digest_type, ChallengesAmount> hash_results;
                        std::array<typename FieldType::value_type, ChallengesAmount> result;

                        for (std::size_t i = 0; i < ChallengesAmount; i++) {

                            // acc(ChallengeId + i);
                            hash_results[i] = accumulators::extract::hash<hash_type>(acc);
                        }

                        return result;
                    }
                };

                template<typename Hash, typename Enable = void>
                struct fiat_shamir_heuristic_sequential
                {
                    typedef Hash hash_type;

                    fiat_shamir_heuristic_sequential() : state(hash<hash_type>({0})) {
                    }

                    template<typename InputRange>
                    fiat_shamir_heuristic_sequential(const InputRange &r) : state(hash<hash_type>(r)) {
                    }

                    template<typename InputIterator>
                    fiat_shamir_heuristic_sequential(InputIterator first, InputIterator last) :
                        state(hash<hash_type>(first, last)) {
                    }

                    template<typename InputRange>
                    void operator()(const InputRange &r) {
                        auto acc_convertible = hash<hash_type>(state);
                        state = accumulators::extract::hash<hash_type>(
                            hash<hash_type>(r, static_cast<accumulator_set<hash_type> &>(acc_convertible)));
                    }

                    template<typename InputIterator>
                    void operator()(InputIterator first, InputIterator last) {
                        auto acc_convertible = hash<hash_type>(state);
                        state = accumulators::extract::hash<hash_type>(
                            hash<hash_type>(first, last, static_cast<accumulator_set<hash_type> &>(acc_convertible)));
                    }

                    template<typename Field>
                    // typename std::enable_if<(Hash::digest_bits >= Field::modulus_bits),
                    //                         typename Field::value_type>::type
                    typename Field::value_type challenge() {

                        state = hash<hash_type>(state);
                        nil::marshalling::status_type status;
                        nil::crypto3::multiprecision::cpp_int raw_result = nil::marshalling::pack(state, status);

                        return raw_result;
                    }

                    template<typename Integral>
                    Integral int_challenge() {

                        state = hash<hash_type>(state);
                        nil::marshalling::status_type status;
                        Integral raw_result = nil::marshalling::pack(state, status);

                        return raw_result;
                    }

                    template<typename Field, std::size_t N>
                    // typename std::enable_if<(Hash::digest_bits >= Field::modulus_bits),
                    //                         std::array<typename Field::value_type, N>>::type
                    std::array<typename Field::value_type, N> challenges() {

                        std::array<typename Field::value_type, N> result;
                        for (auto &ch : result) {
                            ch = challenge<Field>();
                        }

                        return result;
                    }

                private:
                    typename hash_type::digest_type state;
                };

                // Specialize for posseidon.
                template<typename Hash>
                struct fiat_shamir_heuristic_sequential<
                        Hash,
                        typename std::enable_if_t<crypto3::hashes::is_poseidon<Hash>::value>> {

                    typedef Hash hash_type;

                    fiat_shamir_heuristic_sequential() : state(hash<hash_type>(Hash::digest_type::zero())) {
                    }

                    template<typename InputRange>
                    fiat_shamir_heuristic_sequential(const InputRange &r) : state(hash<hash_type>(r)) {
                    }

                    template<typename InputIterator>
                    fiat_shamir_heuristic_sequential(InputIterator first, InputIterator last) :
                        state(hash<hash_type>(first, last)) {
                    }

                    void operator()(const typename hash_type::digest_type input){
                        auto tmp = pair_hash(state, state);
                        state = pair_hash(input, tmp);
                    }

                    template<typename InputRange>
                    void operator()(const InputRange &r) {
                        BOOST_ASSERT_MSG(false, "Not supported");
                    }

                    template<typename InputIterator>
                    void operator()(InputIterator first, InputIterator last) {
                        BOOST_ASSERT_MSG(false, "Not supported");
                    }

                    template<typename Field>
                    typename Field::value_type challenge() {
                        state = pair_hash(state, state);
                        return state;
                    }

                    template<typename Integral>
                    Integral int_challenge() {

                        state = pair_hash(state, state);

                        Integral raw_result = state.data.template convert_to<Integral>();

                        return raw_result;
                    }

                    template<typename Field, std::size_t N>
                    std::array<typename Field::value_type, N> challenges() {

                        std::array<typename Field::value_type, N> result;
                        for (auto &ch : result) {
                            ch = challenge<Field>();
                        }

                        return result;
                    }

                private:
                    typename hash_type::digest_type state;
                    typename hash_type::digest_type pair_hash(typename hash_type::digest_type a1, typename hash_type::digest_type a2){
                        using field_type = nil::crypto3::algebra::curves::pallas::base_field_type;
                        using poseidon_policy = nil::crypto3::hashes::detail::mina_poseidon_policy<field_type>;
                        using permutation_type = nil::crypto3::hashes::detail::poseidon_permutation<poseidon_policy>;
                        using state_type = typename permutation_type::state_type;

                        std::vector<typename field_type::value_type> a = {0, a1, a2};

                        state_type poseidon_state;
                        std::copy(a.begin(), a.end(), poseidon_state.begin());
                        permutation_type::permute(poseidon_state);

                        std::vector<typename field_type::value_type> result(3);
                        std::copy(poseidon_state.begin(), poseidon_state.end(), result.begin());
                        return result[2];
                    }
                };

            }    // namespace transcript
        }        // namespace zk
    }            // namespace crypto3
}    // namespace nil

#endif    // CRYPTO3_ZK_TRANSCRIPT_FIAT_SHAMIR_HEURISTIC_HPP
