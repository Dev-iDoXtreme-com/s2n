/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "tls/s2n_kem_preferences.h"

#include "crypto/s2n_fips.h"
#include "crypto/s2n_pq.h"
#include "s2n_test.h"
#include "tls/s2n_tls_parameters.h"

int main(int argc, char **argv)
{
    BEGIN_TEST();
    EXPECT_SUCCESS(s2n_disable_tls13_in_test());

    EXPECT_FALSE(s2n_kem_preferences_includes_tls13_kem_group(&kem_preferences_null, TLS_PQ_KEM_GROUP_ID_SECP256R1_MLKEM_768));
    EXPECT_FALSE(s2n_kem_preferences_includes_tls13_kem_group(&kem_preferences_null, TLS_PQ_KEM_GROUP_ID_X25519_MLKEM_768));
    EXPECT_FALSE(s2n_kem_preferences_includes_tls13_kem_group(&kem_preferences_null, TLS_PQ_KEM_GROUP_ID_SECP384R1_MLKEM_1024));
    EXPECT_FALSE(s2n_kem_preferences_includes_tls13_kem_group(&kem_preferences_null, TLS_PQ_KEM_GROUP_ID_X25519_KYBER_512_R3));
    EXPECT_FALSE(s2n_kem_preferences_includes_tls13_kem_group(&kem_preferences_null, TLS_PQ_KEM_GROUP_ID_X25519_KYBER_768_R3));
    EXPECT_FALSE(s2n_kem_preferences_includes_tls13_kem_group(&kem_preferences_null, TLS_PQ_KEM_GROUP_ID_SECP256R1_KYBER_512_R3));
    EXPECT_FALSE(s2n_kem_preferences_includes_tls13_kem_group(&kem_preferences_null, TLS_PQ_KEM_GROUP_ID_SECP256R1_KYBER_768_R3));
    EXPECT_FALSE(s2n_kem_preferences_includes_tls13_kem_group(&kem_preferences_null, TLS_PQ_KEM_GROUP_ID_SECP384R1_KYBER_768_R3));
    EXPECT_FALSE(s2n_kem_preferences_includes_tls13_kem_group(&kem_preferences_null, TLS_PQ_KEM_GROUP_ID_SECP521R1_KYBER_1024_R3));

    {
        const struct s2n_kem_preferences test_prefs = {
            .kem_count = 0,
            .kems = NULL,
            .tls13_kem_group_count = S2N_KEM_GROUPS_COUNT,
            .tls13_kem_groups = ALL_SUPPORTED_KEM_GROUPS,
        };

        EXPECT_TRUE(s2n_kem_preferences_includes_tls13_kem_group(&test_prefs, TLS_PQ_KEM_GROUP_ID_SECP256R1_MLKEM_768));
        EXPECT_TRUE(s2n_kem_preferences_includes_tls13_kem_group(&test_prefs, TLS_PQ_KEM_GROUP_ID_X25519_MLKEM_768));
        EXPECT_TRUE(s2n_kem_preferences_includes_tls13_kem_group(&test_prefs, TLS_PQ_KEM_GROUP_ID_SECP384R1_MLKEM_1024));
        EXPECT_TRUE(s2n_kem_preferences_includes_tls13_kem_group(&test_prefs, TLS_PQ_KEM_GROUP_ID_X25519_KYBER_512_R3));
        EXPECT_TRUE(s2n_kem_preferences_includes_tls13_kem_group(&test_prefs, TLS_PQ_KEM_GROUP_ID_X25519_KYBER_768_R3));
        EXPECT_TRUE(s2n_kem_preferences_includes_tls13_kem_group(&test_prefs, TLS_PQ_KEM_GROUP_ID_SECP256R1_KYBER_512_R3));
        EXPECT_TRUE(s2n_kem_preferences_includes_tls13_kem_group(&test_prefs, TLS_PQ_KEM_GROUP_ID_SECP256R1_KYBER_768_R3));
        EXPECT_TRUE(s2n_kem_preferences_includes_tls13_kem_group(&test_prefs, TLS_PQ_KEM_GROUP_ID_SECP384R1_KYBER_768_R3));
        EXPECT_TRUE(s2n_kem_preferences_includes_tls13_kem_group(&test_prefs, TLS_PQ_KEM_GROUP_ID_SECP521R1_KYBER_1024_R3));

        if (s2n_libcrypto_supports_evp_kem()) {
            EXPECT_TRUE(s2n_kem_group_is_available(&s2n_secp256r1_kyber_512_r3));
            EXPECT_TRUE(s2n_kem_group_is_available(&s2n_secp256r1_kyber_768_r3));
            EXPECT_TRUE(s2n_kem_group_is_available(&s2n_secp384r1_kyber_768_r3));
            EXPECT_TRUE(s2n_kem_group_is_available(&s2n_secp521r1_kyber_1024_r3));

            if (s2n_is_evp_apis_supported()) {
                EXPECT_TRUE(s2n_kem_group_is_available(&s2n_x25519_kyber_512_r3));
                EXPECT_TRUE(s2n_kem_group_is_available(&s2n_x25519_kyber_768_r3));
            } else {
                EXPECT_FALSE(s2n_kem_group_is_available(&s2n_x25519_kyber_512_r3));
                EXPECT_FALSE(s2n_kem_group_is_available(&s2n_x25519_kyber_768_r3));
            }

            if (s2n_libcrypto_supports_mlkem()) {
                EXPECT_TRUE(s2n_kem_group_is_available(&s2n_secp256r1_mlkem_768));
                EXPECT_TRUE(s2n_kem_group_is_available(&s2n_secp384r1_mlkem_1024));
                if (s2n_is_evp_apis_supported()) {
                    EXPECT_TRUE(s2n_kem_group_is_available(&s2n_x25519_mlkem_768));
                } else {
                    EXPECT_FALSE(s2n_kem_group_is_available(&s2n_x25519_mlkem_768));
                }
            }
        } else {
            EXPECT_FALSE(s2n_kem_group_is_available(&s2n_secp256r1_kyber_512_r3));
            EXPECT_FALSE(s2n_kem_group_is_available(&s2n_x25519_kyber_512_r3));
            EXPECT_FALSE(s2n_kem_group_is_available(&s2n_x25519_kyber_768_r3));
            EXPECT_FALSE(s2n_kem_group_is_available(&s2n_secp256r1_kyber_768_r3));
            EXPECT_FALSE(s2n_kem_group_is_available(&s2n_secp384r1_kyber_768_r3));
            EXPECT_FALSE(s2n_kem_group_is_available(&s2n_secp521r1_kyber_1024_r3));
            EXPECT_FALSE(s2n_kem_group_is_available(&s2n_secp256r1_mlkem_768));
            EXPECT_FALSE(s2n_kem_group_is_available(&s2n_x25519_mlkem_768));
            EXPECT_FALSE(s2n_kem_group_is_available(&s2n_secp384r1_mlkem_1024));
        }
    };

    END_TEST();
}
