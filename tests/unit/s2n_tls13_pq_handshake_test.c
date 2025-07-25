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

#include "api/s2n.h"
#include "crypto/s2n_pq.h"
#include "s2n_test.h"
#include "testlib/s2n_testlib.h"
#include "tls/s2n_ecc_preferences.h"
#include "tls/s2n_handshake.h"
#include "tls/s2n_kem_preferences.h"
#include "tls/s2n_security_policies.h"

/* Include C file directly to access static functions */
#include "tls/s2n_handshake_io.c"

const struct s2n_kem_group *s2n_get_predicted_negotiated_kem_group(const struct s2n_kem_preferences *client_prefs, const struct s2n_kem_preferences *server_prefs)
{
    PTR_ENSURE_REF(client_prefs);
    PTR_ENSURE_REF(server_prefs);

    /* Client will offer their highest priority PQ KeyShare in their ClientHello. This PQ KeyShare
     * will be most preferred since it can be negotiated in 1-RTT (even if there are other mutually
     * supported PQ KeyShares that the server would prefer over this one but would require 2-RTT's).
     */
    const struct s2n_kem_group *client_default = client_prefs->tls13_kem_groups[0];
    PTR_ENSURE_REF(client_default);

    for (int i = 0; i < server_prefs->tls13_kem_group_count; i++) {
        const struct s2n_kem_group *server_group = server_prefs->tls13_kem_groups[i];
        PTR_ENSURE_REF(server_group);
        if (s2n_kem_group_is_available(client_default) && s2n_kem_group_is_available(server_group)
                && client_default->iana_id == server_group->iana_id
                && s2n_kem_group_is_available(client_default)) {
            return client_default;
        }
    }

    /* Otherwise, if the client's default isn't supported, and a 2-RTT PQ handshake is required, the server will choose
     * whichever mutually supported PQ KeyShare that is highest on the server's preference list. */
    for (int i = 0; i < server_prefs->tls13_kem_group_count; i++) {
        const struct s2n_kem_group *server_group = server_prefs->tls13_kem_groups[i];

        /* j starts at 1 since we already checked client_prefs->tls13_kem_groups[0] above */
        for (int j = 1; j < client_prefs->tls13_kem_group_count; j++) {
            const struct s2n_kem_group *client_group = client_prefs->tls13_kem_groups[j];
            PTR_ENSURE_REF(client_group);
            PTR_ENSURE_REF(server_group);
            if (s2n_kem_group_is_available(client_group) && s2n_kem_group_is_available(server_group)
                    && client_group->iana_id == server_group->iana_id
                    && s2n_kem_group_is_available(client_group)) {
                return client_group;
            }
        }
    }

    return NULL;
}

const struct s2n_ecc_named_curve *s2n_get_predicted_negotiated_ecdhe_curve(const struct s2n_security_policy *client_sec_policy,
        const struct s2n_security_policy *server_sec_policy)
{
    PTR_ENSURE_REF(client_sec_policy);
    PTR_ENSURE_REF(server_sec_policy);

    /* Client will offer their highest priority ECDHE KeyShare in their ClientHello. This KeyShare
     * will be most preferred since it can be negotiated in 1-RTT (even if there are other mutually
     * supported ECDHE KeyShares that the server would prefer over this one but would require 2-RTT's).
     */
    const struct s2n_ecc_named_curve *client_default = client_sec_policy->ecc_preferences->ecc_curves[0];
    PTR_ENSURE_REF(client_default);

    for (int i = 0; i < server_sec_policy->ecc_preferences->count; i++) {
        const struct s2n_ecc_named_curve *server_curve = server_sec_policy->ecc_preferences->ecc_curves[i];
        PTR_ENSURE_REF(server_curve);
        if (server_curve->iana_id == client_default->iana_id) {
            return client_default;
        }
    }

    /* Otherwise, if the client's default isn't supported, and a 2-RTT handshake is required, the server will choose
     * whichever mutually supported PQ KeyShare that is highest on the server's preference list. */
    for (int i = 0; i < server_sec_policy->ecc_preferences->count; i++) {
        const struct s2n_ecc_named_curve *server_curve = server_sec_policy->ecc_preferences->ecc_curves[i];

        /* j starts at 1 since we already checked client_sec_policy->ecc_preferences->ecc_curves[0] above */
        for (int j = 1; j < client_sec_policy->ecc_preferences->count; j++) {
            const struct s2n_ecc_named_curve *client_curve = client_sec_policy->ecc_preferences->ecc_curves[j];
            PTR_ENSURE_REF(client_curve);
            PTR_ENSURE_REF(server_curve);
            if (client_curve->iana_id == server_curve->iana_id) {
                return client_curve;
            }
        }
    }

    return NULL;
}

int s2n_test_tls13_pq_handshake(const struct s2n_security_policy *client_sec_policy,
        const struct s2n_security_policy *server_sec_policy, const struct s2n_kem_group *expected_kem_group,
        const struct s2n_ecc_named_curve *expected_curve, bool hrr_expected, bool len_prefix_expected)
{
    /* XOR check: can expect to negotiate either a KEM group, or a classic EC curve, but not both/neither */
    POSIX_ENSURE((expected_kem_group == NULL) != (expected_curve == NULL), S2N_ERR_SAFETY);

    /* Set up connections */
    struct s2n_connection *client_conn = NULL, *server_conn = NULL;
    POSIX_ENSURE_REF(client_conn = s2n_connection_new(S2N_CLIENT));
    POSIX_ENSURE_REF(server_conn = s2n_connection_new(S2N_SERVER));

    struct s2n_config *client_config = NULL, *server_config = NULL;
    POSIX_ENSURE_REF(client_config = s2n_config_new());
    POSIX_ENSURE_REF(server_config = s2n_config_new());

    char cert_chain[S2N_MAX_TEST_PEM_SIZE] = { 0 }, private_key[S2N_MAX_TEST_PEM_SIZE] = { 0 };
    struct s2n_cert_chain_and_key *chain_and_key = NULL;
    POSIX_ENSURE_REF(chain_and_key = s2n_cert_chain_and_key_new());
    POSIX_GUARD(s2n_read_test_pem(S2N_ECDSA_P384_PKCS1_CERT_CHAIN, cert_chain, S2N_MAX_TEST_PEM_SIZE));
    POSIX_GUARD(s2n_read_test_pem(S2N_ECDSA_P384_PKCS1_KEY, private_key, S2N_MAX_TEST_PEM_SIZE));
    POSIX_GUARD(s2n_cert_chain_and_key_load_pem(chain_and_key, cert_chain, private_key));
    POSIX_GUARD(s2n_config_add_cert_chain_and_key_to_store(client_config, chain_and_key));
    POSIX_GUARD(s2n_config_add_cert_chain_and_key_to_store(server_config, chain_and_key));

    POSIX_GUARD(s2n_connection_set_config(client_conn, client_config));
    POSIX_GUARD(s2n_connection_set_config(server_conn, server_config));

    struct s2n_stuffer client_to_server = { 0 }, server_to_client = { 0 };
    POSIX_GUARD(s2n_stuffer_growable_alloc(&client_to_server, 2048));
    POSIX_GUARD(s2n_stuffer_growable_alloc(&server_to_client, 2048));

    POSIX_GUARD(s2n_connection_set_io_stuffers(&server_to_client, &client_to_server, client_conn));
    POSIX_GUARD(s2n_connection_set_io_stuffers(&client_to_server, &server_to_client, server_conn));

    client_conn->security_policy_override = client_sec_policy;
    server_conn->security_policy_override = server_sec_policy;

    /* Client sends ClientHello */
    POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(client_conn), CLIENT_HELLO);
    POSIX_GUARD(s2n_handshake_write_io(client_conn));

    POSIX_ENSURE_EQ(client_conn->actual_protocol_version, S2N_TLS13);
    POSIX_ENSURE_EQ(server_conn->actual_protocol_version, 0); /* Won't get set until after server reads ClientHello */
    POSIX_ENSURE_EQ(client_conn->handshake.handshake_type, INITIAL);

    /* Server reads ClientHello */
    POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(server_conn), CLIENT_HELLO);
    POSIX_GUARD(s2n_handshake_read_io(server_conn));

    POSIX_ENSURE_EQ(server_conn->actual_protocol_version, S2N_TLS13); /* Server is now on TLS13 */

    /* Assert that the server chose the correct group */
    if (expected_kem_group) {
        /* Client should always determine whether the Hybrid KEM used len_prefixed format, and server should match client's behavior. */
        POSIX_ENSURE_EQ(len_prefix_expected, client_conn->kex_params.client_kem_group_params.kem_params.len_prefixed);
        POSIX_ENSURE_EQ(len_prefix_expected, s2n_tls13_client_must_use_hybrid_kem_length_prefix(client_sec_policy->kem_preferences));
        POSIX_ENSURE_EQ(server_conn->kex_params.client_kem_group_params.kem_params.len_prefixed, client_conn->kex_params.client_kem_group_params.kem_params.len_prefixed);

        POSIX_ENSURE_EQ(expected_kem_group, server_conn->kex_params.server_kem_group_params.kem_group);
        POSIX_ENSURE_EQ(expected_kem_group->kem, server_conn->kex_params.server_kem_group_params.kem_params.kem);
        POSIX_ENSURE_EQ(expected_kem_group->curve, server_conn->kex_params.server_kem_group_params.ecc_params.negotiated_curve);
        POSIX_ENSURE_EQ(NULL, server_conn->kex_params.server_ecc_evp_params.negotiated_curve);
    } else {
        POSIX_ENSURE_EQ(NULL, server_conn->kex_params.server_kem_group_params.kem_group);
        POSIX_ENSURE_EQ(NULL, server_conn->kex_params.server_kem_group_params.kem_params.kem);
        POSIX_ENSURE_EQ(NULL, server_conn->kex_params.server_kem_group_params.ecc_params.negotiated_curve);
        POSIX_ENSURE_EQ(expected_curve, server_conn->kex_params.server_ecc_evp_params.negotiated_curve);
    }

    /* Server sends ServerHello or HRR */
    POSIX_GUARD(s2n_conn_set_handshake_type(server_conn));
    POSIX_ENSURE_EQ(hrr_expected, s2n_handshake_type_check_tls13_flag(server_conn, HELLO_RETRY_REQUEST));
    POSIX_GUARD(s2n_handshake_write_io(server_conn));

    /* Server sends CCS */
    POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(server_conn), SERVER_CHANGE_CIPHER_SPEC);
    POSIX_GUARD(s2n_handshake_write_io(server_conn));

    if (hrr_expected) {
        /* Client reads HRR */
        POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(client_conn), SERVER_HELLO);
        POSIX_GUARD(s2n_handshake_read_io(client_conn));
        POSIX_GUARD(s2n_conn_set_handshake_type(client_conn));
        POSIX_ENSURE_NE(0, client_conn->handshake.handshake_type & HELLO_RETRY_REQUEST);

        /* Client reads CCS */
        POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(client_conn), CLIENT_CHANGE_CIPHER_SPEC);
        POSIX_GUARD(s2n_handshake_read_io(client_conn));

        /* Client sends CCS and new ClientHello */
        POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(client_conn), CLIENT_CHANGE_CIPHER_SPEC);
        POSIX_GUARD(s2n_handshake_write_io(client_conn));
        POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(client_conn), CLIENT_HELLO);
        POSIX_GUARD(s2n_handshake_write_io(client_conn));

        /* Server reads CCS (doesn't change state machine) */
        POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(server_conn), CLIENT_HELLO);
        POSIX_GUARD(s2n_handshake_read_io(server_conn));

        /* Server reads new ClientHello */
        POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(server_conn), CLIENT_HELLO);
        POSIX_GUARD(s2n_handshake_read_io(server_conn));

        /* Server sends ServerHello */
        POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(server_conn), SERVER_HELLO);
        POSIX_GUARD(s2n_handshake_write_io(server_conn));
    }

    /* Client reads ServerHello */
    POSIX_ENSURE_EQ(s2n_conn_get_current_message_type(client_conn), SERVER_HELLO);
    POSIX_GUARD(s2n_handshake_read_io(client_conn));

    /* We've gotten far enough in the handshake that both client and server should have
     * derived the shared secrets, so we don't send/receive any more messages. */

    /* Assert that the correct group was negotiated (we re-check the server group to assert that
     * nothing unexpected changed between then and now while e.g. processing HRR) */
    if (expected_kem_group) {
        POSIX_ENSURE_EQ(expected_kem_group, client_conn->kex_params.server_kem_group_params.kem_group);
        POSIX_ENSURE_EQ(expected_kem_group->kem, client_conn->kex_params.server_kem_group_params.kem_params.kem);
        POSIX_ENSURE_EQ(expected_kem_group->curve, client_conn->kex_params.server_kem_group_params.ecc_params.negotiated_curve);
        POSIX_ENSURE_EQ(NULL, client_conn->kex_params.server_ecc_evp_params.negotiated_curve);

        POSIX_ENSURE_EQ(expected_kem_group, server_conn->kex_params.server_kem_group_params.kem_group);
        POSIX_ENSURE_EQ(expected_kem_group->kem, server_conn->kex_params.server_kem_group_params.kem_params.kem);
        POSIX_ENSURE_EQ(expected_kem_group->curve, server_conn->kex_params.server_kem_group_params.ecc_params.negotiated_curve);
        POSIX_ENSURE_EQ(NULL, server_conn->kex_params.server_ecc_evp_params.negotiated_curve);

        /* Ensure s2n_connection_get_kem_group_name() gives the correct answer for both client and server */
        POSIX_ENSURE_EQ(strlen(expected_kem_group->name), strlen(s2n_connection_get_kem_group_name(server_conn)));
        POSIX_ENSURE_EQ(memcmp(expected_kem_group->name, s2n_connection_get_kem_group_name(server_conn), strlen(expected_kem_group->name)), 0);
        POSIX_ENSURE_EQ(strlen(expected_kem_group->name), strlen(s2n_connection_get_kem_group_name(client_conn)));
        POSIX_ENSURE_EQ(memcmp(expected_kem_group->name, s2n_connection_get_kem_group_name(client_conn), strlen(expected_kem_group->name)), 0);

        /* Ensure s2n_connection_get_key_exchange_group() gives the correct answer for both client and server */
        const char *server_group_name = NULL;
        const char *client_group_name = NULL;
        POSIX_GUARD(s2n_connection_get_key_exchange_group(server_conn, &server_group_name));
        POSIX_GUARD(s2n_connection_get_key_exchange_group(client_conn, &client_group_name));
        POSIX_ENSURE_EQ(strlen(expected_kem_group->name), strlen(server_group_name));
        POSIX_ENSURE_EQ(memcmp(expected_kem_group->name, server_group_name, strlen(expected_kem_group->name)), 0);
        POSIX_ENSURE_EQ(strlen(expected_kem_group->name), strlen(client_group_name));
        POSIX_ENSURE_EQ(memcmp(expected_kem_group->name, client_group_name, strlen(expected_kem_group->name)), 0);
    } else {
        POSIX_ENSURE_EQ(NULL, client_conn->kex_params.server_kem_group_params.kem_group);
        POSIX_ENSURE_EQ(NULL, client_conn->kex_params.server_kem_group_params.kem_params.kem);
        POSIX_ENSURE_EQ(NULL, client_conn->kex_params.server_kem_group_params.ecc_params.negotiated_curve);
        POSIX_ENSURE_EQ(expected_curve, client_conn->kex_params.server_ecc_evp_params.negotiated_curve);

        POSIX_ENSURE_EQ(NULL, server_conn->kex_params.server_kem_group_params.kem_group);
        POSIX_ENSURE_EQ(NULL, server_conn->kex_params.server_kem_group_params.kem_params.kem);
        POSIX_ENSURE_EQ(NULL, server_conn->kex_params.server_kem_group_params.ecc_params.negotiated_curve);
        POSIX_ENSURE_EQ(expected_curve, server_conn->kex_params.server_ecc_evp_params.negotiated_curve);

        /* Ensure s2n_connection_get_curve() gives the correct answer for both client and server */
        POSIX_ENSURE_EQ(strlen(expected_curve->name), strlen(s2n_connection_get_curve(server_conn)));
        POSIX_ENSURE_EQ(memcmp(expected_curve->name, s2n_connection_get_curve(server_conn), strlen(expected_curve->name)), 0);
        POSIX_ENSURE_EQ(strlen(expected_curve->name), strlen(s2n_connection_get_curve(client_conn)));
        POSIX_ENSURE_EQ(memcmp(expected_curve->name, s2n_connection_get_curve(client_conn), strlen(expected_curve->name)), 0);

        /* Ensure s2n_connection_get_key_exchange_group() gives the correct answer for both client and server */
        const char *server_group_name = NULL;
        const char *client_group_name = NULL;
        POSIX_GUARD(s2n_connection_get_key_exchange_group(server_conn, &server_group_name));
        POSIX_GUARD(s2n_connection_get_key_exchange_group(client_conn, &client_group_name));
        POSIX_ENSURE_EQ(strlen(expected_curve->name), strlen(server_group_name));
        POSIX_ENSURE_EQ(memcmp(expected_curve->name, server_group_name, strlen(expected_curve->name)), 0);
        POSIX_ENSURE_EQ(strlen(expected_curve->name), strlen(client_group_name));
        POSIX_ENSURE_EQ(memcmp(expected_curve->name, client_group_name, strlen(expected_curve->name)), 0);
    }

    /* Verify basic properties of secrets */
    s2n_tls13_connection_keys(server_secret_info, server_conn);
    s2n_tls13_connection_keys(client_secret_info, client_conn);
    POSIX_ENSURE_EQ(server_conn->secure->cipher_suite, client_conn->secure->cipher_suite);
    if (server_conn->secure->cipher_suite == &s2n_tls13_aes_256_gcm_sha384) {
        POSIX_ENSURE_EQ(server_secret_info.size, 48);
        POSIX_ENSURE_EQ(client_secret_info.size, 48);
    } else {
        POSIX_ENSURE_EQ(server_secret_info.size, 32);
        POSIX_ENSURE_EQ(client_secret_info.size, 32);
    }

    /* Verify secrets aren't just zero'ed memory */
    uint8_t all_zeros[S2N_TLS13_SECRET_MAX_LEN] = { 0 };
    POSIX_CHECKED_MEMSET((void *) all_zeros, 0, S2N_TLS13_SECRET_MAX_LEN);
    struct s2n_tls13_secrets *client_secrets = &client_conn->secrets.version.tls13;
    struct s2n_tls13_secrets *server_secrets = &server_conn->secrets.version.tls13;
    POSIX_ENSURE_EQ(server_secret_info.size, client_secret_info.size);
    uint8_t size = server_secret_info.size;
    POSIX_ENSURE_EQ(client_conn->secrets.extract_secret_type, S2N_HANDSHAKE_SECRET);
    POSIX_ENSURE_NE(0, memcmp(all_zeros, client_secrets->extract_secret, size));
    POSIX_ENSURE_NE(0, memcmp(all_zeros, client_secrets->client_handshake_secret, size));
    POSIX_ENSURE_NE(0, memcmp(all_zeros, client_secrets->server_handshake_secret, size));
    POSIX_ENSURE_EQ(server_conn->secrets.extract_secret_type, S2N_HANDSHAKE_SECRET);
    POSIX_ENSURE_NE(0, memcmp(all_zeros, server_secrets->extract_secret, size));
    POSIX_ENSURE_NE(0, memcmp(all_zeros, server_secrets->client_handshake_secret, size));
    POSIX_ENSURE_NE(0, memcmp(all_zeros, server_secrets->server_handshake_secret, size));

    /* Verify client and server secrets are equal to each other */
    POSIX_ENSURE_EQ(0, memcmp(server_secrets->extract_secret, client_secrets->extract_secret, size));
    POSIX_ENSURE_EQ(0, memcmp(server_secrets->client_handshake_secret, client_secrets->client_handshake_secret, size));
    POSIX_ENSURE_EQ(0, memcmp(server_secrets->server_handshake_secret, client_secrets->server_handshake_secret, size));

    /* Clean up */
    POSIX_GUARD(s2n_stuffer_free(&client_to_server));
    POSIX_GUARD(s2n_stuffer_free(&server_to_client));

    POSIX_GUARD(s2n_connection_free(client_conn));
    POSIX_GUARD(s2n_connection_free(server_conn));

    POSIX_GUARD(s2n_cert_chain_and_key_free(chain_and_key));
    POSIX_GUARD(s2n_config_free(server_config));
    POSIX_GUARD(s2n_config_free(client_config));

    return S2N_SUCCESS;
}

int main()
{
    BEGIN_TEST();

    if (!s2n_is_tls13_fully_supported()) {
        END_TEST();
    }

    /* Additional KEM preferences/security policies to test against. These policies can only be used
     * as the server's policy in this test: when generating the ClientHello, the client relies on
     * the security_policy_selection[] array (in s2n_security_policies.c) to determine if it should
     * write the supported_groups extension. Because these unofficial policies don't exist in that
     * array, the supported_groups extension won't get sent and the handshake won't complete as expected. */

    /* Kyber */
    const struct s2n_kem_group *kyber_test_groups[] = {
        &s2n_x25519_kyber_512_r3,
        &s2n_secp256r1_kyber_512_r3,
        &s2n_secp256r1_kyber_768_r3,
        &s2n_secp384r1_kyber_768_r3,
        &s2n_secp521r1_kyber_1024_r3,
        &s2n_x25519_kyber_768_r3,
    };

    const struct s2n_kem_preferences kyber_test_prefs_draft0 = {
        .kem_count = 0,
        .kems = NULL,
        .tls13_kem_group_count = s2n_array_len(kyber_test_groups),
        .tls13_kem_groups = kyber_test_groups,
        .tls13_pq_hybrid_draft_revision = 0
    };

    const struct s2n_security_policy kyber_test_policy_draft0 = {
        .minimum_protocol_version = S2N_TLS10,
        .cipher_preferences = &cipher_preferences_20190801,
        .kem_preferences = &kyber_test_prefs_draft0,
        .signature_preferences = &s2n_signature_preferences_20200207,
        .ecc_preferences = &s2n_ecc_preferences_20200310,
    };

    const struct s2n_kem_preferences kyber_test_prefs_draft5 = {
        .kem_count = 0,
        .kems = NULL,
        .tls13_kem_group_count = s2n_array_len(kyber_test_groups),
        .tls13_kem_groups = kyber_test_groups,
        .tls13_pq_hybrid_draft_revision = 5
    };

    const struct s2n_security_policy kyber_test_policy_draft5 = {
        .minimum_protocol_version = S2N_TLS10,
        .cipher_preferences = &cipher_preferences_20190801,
        .kem_preferences = &kyber_test_prefs_draft5,
        .signature_preferences = &s2n_signature_preferences_20200207,
        .ecc_preferences = &s2n_ecc_preferences_20200310,
    };

    const struct s2n_kem_group *kyber768_test_kem_groups[] = {
        &s2n_secp384r1_kyber_768_r3,
        &s2n_secp256r1_kyber_512_r3,
    };

    const struct s2n_kem_preferences kyber768_test_prefs = {
        .kem_count = 0,
        .kems = NULL,
        .tls13_kem_group_count = s2n_array_len(kyber768_test_kem_groups),
        .tls13_kem_groups = kyber768_test_kem_groups,
        .tls13_pq_hybrid_draft_revision = 5,
    };

    const struct s2n_security_policy kyber768_test_policy = {
        .minimum_protocol_version = S2N_TLS13,
        .cipher_preferences = &cipher_preferences_20190801,
        .kem_preferences = &kyber768_test_prefs,
        .signature_preferences = &s2n_signature_preferences_20200207,
        .ecc_preferences = &s2n_ecc_preferences_20201021,
    };

    const struct s2n_kem_group *kyber1024_test_kem_groups[] = {
        &s2n_secp521r1_kyber_1024_r3,
        &s2n_secp256r1_kyber_512_r3,
    };

    const struct s2n_kem_preferences kyber1024_test_prefs = {
        .kem_count = 0,
        .kems = NULL,
        .tls13_kem_group_count = s2n_array_len(kyber1024_test_kem_groups),
        .tls13_kem_groups = kyber1024_test_kem_groups,
        .tls13_pq_hybrid_draft_revision = 5,
    };

    const struct s2n_security_policy kyber1024_test_policy = {
        .minimum_protocol_version = S2N_TLS13,
        .cipher_preferences = &cipher_preferences_20190801,
        .kem_preferences = &kyber1024_test_prefs,
        .signature_preferences = &s2n_signature_preferences_20200207,
        .ecc_preferences = &s2n_ecc_preferences_20201021,
    };

    const struct s2n_kem_group *mlkem768_test_groups[] = {
        &s2n_x25519_mlkem_768,
        &s2n_secp256r1_mlkem_768,
    };

    const struct s2n_kem_preferences mlkem768_test_prefs = {
        .kem_count = 0,
        .kems = NULL,
        .tls13_kem_group_count = s2n_array_len(mlkem768_test_groups),
        .tls13_kem_groups = mlkem768_test_groups,
        .tls13_pq_hybrid_draft_revision = 5
    };

    const struct s2n_security_policy mlkem768_test_policy = {
        .minimum_protocol_version = S2N_TLS13,
        .cipher_preferences = &cipher_preferences_20190801,
        .kem_preferences = &mlkem768_test_prefs,
        .signature_preferences = &s2n_signature_preferences_20200207,
        .ecc_preferences = &s2n_ecc_preferences_20240603,
    };

    const struct s2n_kem_group *mlkem1024_test_groups[] = {
        &s2n_secp384r1_mlkem_1024,
    };

    const struct s2n_kem_preferences mlkem1024_test_prefs = {
        .kem_count = 0,
        .kems = NULL,
        .tls13_kem_group_count = s2n_array_len(mlkem1024_test_groups),
        .tls13_kem_groups = mlkem1024_test_groups,
        .tls13_pq_hybrid_draft_revision = 5
    };

    const struct s2n_security_policy mlkem1024_test_policy = {
        .minimum_protocol_version = S2N_TLS13,
        .cipher_preferences = &cipher_preferences_20190801,
        .kem_preferences = &mlkem1024_test_prefs,
        .signature_preferences = &s2n_signature_preferences_20200207,
        .ecc_preferences = &s2n_ecc_preferences_20240603,
    };

    const struct s2n_security_policy ecc_retry_policy = {
        .minimum_protocol_version = security_policy_pq_tls_1_0_2020_12.minimum_protocol_version,
        .cipher_preferences = security_policy_pq_tls_1_0_2020_12.cipher_preferences,
        .kem_preferences = security_policy_pq_tls_1_0_2020_12.kem_preferences,
        .signature_preferences = security_policy_pq_tls_1_0_2020_12.signature_preferences,
        .ecc_preferences = security_policy_test_tls13_retry.ecc_preferences,
    };

    const struct s2n_ecc_named_curve *default_curve = &s2n_ecc_curve_x25519;

    if (!s2n_is_evp_apis_supported()) {
        default_curve = &s2n_ecc_curve_secp256r1;
    }

    struct pq_handshake_test_vector {
        const struct s2n_security_policy *client_policy;
        const struct s2n_security_policy *server_policy;
        const struct s2n_kem_group *expected_kem_group;
        const struct s2n_ecc_named_curve *expected_curve;
        bool hrr_expected;
        bool len_prefix_expected;
    };

    /* Self talk test with each TLS 1.3 KemGroup we support */
    for (size_t i = 0; i < S2N_KEM_GROUPS_COUNT; i++) {
        const struct s2n_kem_group *kem_group = ALL_SUPPORTED_KEM_GROUPS[i];

        if (kem_group == NULL || !s2n_kem_group_is_available(kem_group)) {
            continue;
        }

        const struct s2n_kem_preferences singleton_test_pref = {
            .kem_count = 0,
            .kems = NULL,
            .tls13_kem_group_count = 1,
            .tls13_kem_groups = &kem_group,
            .tls13_pq_hybrid_draft_revision = 5
        };

        const struct s2n_security_policy singleton_test_policy = {
            .minimum_protocol_version = S2N_TLS13,
            .cipher_preferences = &cipher_preferences_20190801,
            .kem_preferences = &singleton_test_pref,
            .signature_preferences = &s2n_signature_preferences_20200207,
            .ecc_preferences = &s2n_ecc_preferences_20240603,
        };

        const struct pq_handshake_test_vector test_vec = {
            .client_policy = &singleton_test_policy,
            .server_policy = &singleton_test_policy,
            .expected_kem_group = kem_group,
            .expected_curve = NULL,
            .hrr_expected = false,
            .len_prefix_expected = false,
        };

        EXPECT_SUCCESS(s2n_test_tls13_pq_handshake(test_vec.client_policy, test_vec.server_policy,
                test_vec.expected_kem_group, test_vec.expected_curve, test_vec.hrr_expected, test_vec.len_prefix_expected));
    }

    /* ML-KEM is only available on newer versions of AWS-LC. If it's
     * unavailable, we must downgrade the assertions to Kyber or EC. */
    const struct s2n_kem_group *null_if_no_mlkem_768 = &s2n_x25519_mlkem_768;
    const struct s2n_kem_group *null_if_no_mlkem_1024 = &s2n_secp384r1_mlkem_1024;
    const struct s2n_ecc_named_curve *ec_if_no_mlkem = NULL;
    if (!s2n_libcrypto_supports_mlkem()) {
        null_if_no_mlkem_768 = NULL;
        null_if_no_mlkem_1024 = NULL;
        ec_if_no_mlkem = default_curve;
    }

    /* Test vectors that expect to negotiate PQ assume that PQ is enabled in s2n.
     * If PQ is disabled, the expected negotiation outcome is overridden below
     * before performing the handshake test. */
    const struct pq_handshake_test_vector test_vectors[] = {
        {
                .client_policy = &security_policy_pq_tls_1_3_2023_06_01,
                .server_policy = &security_policy_pq_tls_1_0_2021_05_24,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = s2n_pq_is_enabled(),
                .len_prefix_expected = false,
        },
        /* Server and Client both support PQ and TLS 1.3 */
        {
                .client_policy = &security_policy_pq_tls_1_1_2021_05_21,
                .server_policy = &security_policy_pq_tls_1_1_2021_05_21,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },
        {
                .client_policy = &security_policy_pq_tls_1_0_2021_05_22,
                .server_policy = &security_policy_pq_tls_1_0_2021_05_22,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },
        {
                .client_policy = &security_policy_pq_tls_1_0_2021_05_23,
                .server_policy = &security_policy_pq_tls_1_0_2021_05_23,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },
        {
                .client_policy = &security_policy_pq_tls_1_0_2021_05_24,
                .server_policy = &security_policy_pq_tls_1_0_2021_05_24,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },
        {
                .client_policy = &security_policy_pq_tls_1_0_2021_05_26,
                .server_policy = &security_policy_pq_tls_1_0_2021_05_26,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },
        {
                .client_policy = &security_policy_pq_tls_1_0_2023_01_24,
                .server_policy = &security_policy_pq_tls_1_0_2023_01_24,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = false,
        },
        /* Kyber768 should be preferred over 1024, which should be preferred over 512
         * when available. Note that unlike older KEM group preferences, 2023_06_01
         * prefers secp256r1 over x25519 for the hybrid EC.
         */
        {
                .client_policy = &security_policy_pq_tls_1_3_2023_06_01,
                .server_policy = &security_policy_pq_tls_1_3_2023_06_01,
                .expected_kem_group = &s2n_secp256r1_kyber_768_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = false,
        },
        {
                .client_policy = &kyber1024_test_policy,
                .server_policy = &security_policy_pq_tls_1_3_2023_06_01,
                .expected_kem_group = &s2n_secp521r1_kyber_1024_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = false,
        },
        {
                .client_policy = &kyber768_test_policy,
                .server_policy = &security_policy_pq_tls_1_3_2023_06_01,
                .expected_kem_group = &s2n_secp384r1_kyber_768_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = false,
        },
        /* Server supports Kyber768+ parameters, Client only supports Kyber512.
         * Expect Kyber512 to be negotiated if PQ is enabled, else fall back to
         * ECC on hello retry.
         */
        {
                .client_policy = &security_policy_pq_tls_1_1_2021_05_21,
                .server_policy = &security_policy_pq_tls_1_3_2023_06_01,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = !s2n_pq_is_enabled(),
                .len_prefix_expected = true,
        },
        /* Check that we're backwards and forwards compatible with different Hybrid PQ draft revisions*/
        {
                .client_policy = &kyber_test_policy_draft0,
                .server_policy = &kyber_test_policy_draft5,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },
        {
                .client_policy = &kyber_test_policy_draft5,
                .server_policy = &kyber_test_policy_draft0,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = false,
        },
        {
                .client_policy = &security_policy_pq_tls_1_0_2021_05_24,
                .server_policy = &security_policy_pq_tls_1_0_2023_01_24,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },
        {
                .client_policy = &security_policy_pq_tls_1_0_2023_01_24,
                .server_policy = &security_policy_pq_tls_1_0_2021_05_24,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = false,
        },

        /* Server supports all KEM groups; client sends a PQ key share and an EC key
         * share; server chooses to negotiate client's first choice PQ without HRR. */
        {
                .client_policy = &security_policy_pq_tls_1_0_2020_12,
                .server_policy = &security_policy_pq_tls_1_0_2020_12,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },

        /* Server supports only one KEM group and it is the client's first choice;
         * client sends a PQ share and an EC share; server chooses to negotiate PQ
         * without HRR. */
        {
                .client_policy = &security_policy_pq_tls_1_0_2020_12,
                .server_policy = &kyber_test_policy_draft0,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },

        /* Server supports only one KEM group and it is the client's first choice;
         * client sends a PQ share and an EC share; server chooses to negotiate PQ
         * without HRR. */
        {
                .client_policy = &security_policy_pq_tls_1_0_2020_12,
                .server_policy = &kyber_test_policy_draft5,
                .expected_kem_group = &s2n_x25519_kyber_512_r3,
                .expected_curve = NULL,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },

        /* Server does not support PQ; client sends a PQ key share and an EC key share;
         * server should negotiate EC without HRR. */
        {
                .client_policy = &security_policy_pq_tls_1_0_2020_12,
                .server_policy = &security_policy_test_all_tls13,
                .expected_kem_group = NULL,
                .expected_curve = default_curve,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },

        /* Server does not support PQ; client sends a PQ key share, but no EC shares;
         * server should negotiate EC and send HRR. */
        {
                .client_policy = &ecc_retry_policy,
                .server_policy = &security_policy_test_all_tls13,
                .expected_kem_group = NULL,
                .expected_curve = default_curve,
                .hrr_expected = true,
                .len_prefix_expected = true,
        },

        /* Server supports PQ, but client does not. Client sent an EC share,
         * EC should be negotiated without HRR */
        {
                .client_policy = &security_policy_test_all_tls13,
                .server_policy = &security_policy_pq_tls_1_0_2020_12,
                .expected_kem_group = NULL,
                .expected_curve = default_curve,
                .hrr_expected = false,
                .len_prefix_expected = true,
        },

        /* Server supports PQ, but client does not. Client did not send any EC shares,
         * EC should be negotiated after exchanging HRR */
        {
                .client_policy = &security_policy_test_tls13_retry,
                .server_policy = &security_policy_pq_tls_1_0_2020_12,
                .expected_kem_group = NULL,
                .expected_curve = default_curve,
                .hrr_expected = true,
                .len_prefix_expected = true,
        },

        /* Confirm that MLKEM768 is negotiable */
        {
                .client_policy = &mlkem768_test_policy,
                .server_policy = &mlkem768_test_policy,
                .expected_kem_group = null_if_no_mlkem_768,
                .expected_curve = ec_if_no_mlkem,
                .hrr_expected = false,
                .len_prefix_expected = false,
        },

        /* Confirm that MLKEM1024 is negotiable */
        {
                .client_policy = &mlkem1024_test_policy,
                .server_policy = &mlkem1024_test_policy,
                .expected_kem_group = null_if_no_mlkem_1024,
                .expected_curve = ec_if_no_mlkem,
                .hrr_expected = false,
                .len_prefix_expected = false,
        }
    };

    for (size_t i = 0; i < s2n_array_len(test_vectors); i++) {
        const struct pq_handshake_test_vector *vector = &test_vectors[i];
        const struct s2n_security_policy *client_policy = vector->client_policy;
        const struct s2n_security_policy *server_policy = vector->server_policy;
        const struct s2n_kem_group *kem_group = vector->expected_kem_group;
        const struct s2n_ecc_named_curve *curve = vector->expected_curve;
        bool hrr_expected = vector->hrr_expected;
        bool len_prefix_expected = vector->len_prefix_expected;

        if (!s2n_pq_is_enabled()) {
            EXPECT_TRUE(client_policy->ecc_preferences->count > 0);
            const struct s2n_ecc_named_curve *client_default = client_policy->ecc_preferences->ecc_curves[0];
            const struct s2n_ecc_named_curve *predicted_curve = s2n_get_predicted_negotiated_ecdhe_curve(client_policy, server_policy);

            /* If either policy doesn't support the default curve, fall back to p256 as it should
             * be in common with every ECC preference list. */
            if (!s2n_ecc_preferences_includes_curve(client_policy->ecc_preferences, default_curve->iana_id)
                    || !s2n_ecc_preferences_includes_curve(server_policy->ecc_preferences, default_curve->iana_id)) {
                EXPECT_TRUE(s2n_ecc_preferences_includes_curve(client_policy->ecc_preferences, s2n_ecc_curve_secp256r1.iana_id));
                EXPECT_TRUE(s2n_ecc_preferences_includes_curve(server_policy->ecc_preferences, s2n_ecc_curve_secp256r1.iana_id));
                curve = &s2n_ecc_curve_secp256r1;
            }

            /* The client's preferred curve will be a higher priority than the default if both sides
             * support TLS 1.3, and if the client's default can be chosen by the server in 1-RTT. */
            if (s2n_security_policy_supports_tls13(client_policy) && s2n_security_policy_supports_tls13(server_policy)
                    && s2n_ecc_preferences_includes_curve(server_policy->ecc_preferences, client_default->iana_id)) {
                curve = client_default;
            }

            /* Finally, confirm that the expected curve listed in the test vector matches the output of s2n_get_predicted_negotiated_ecdhe_curve() */
            EXPECT_EQUAL(curve->iana_id, predicted_curve->iana_id);
        }

        if (!s2n_kem_group_is_available(kem_group)) {
            kem_group = NULL;
        }

        if (kem_group != NULL) {
            const struct s2n_kem_group *predicted_kem_group = s2n_get_predicted_negotiated_kem_group(client_policy->kem_preferences, server_policy->kem_preferences);
            POSIX_ENSURE_REF(predicted_kem_group);

            /* Confirm that the expected KEM Group listed in the test vector matches the output of
             * s2n_get_predicted_negotiated_kem_group() */
            POSIX_ENSURE_EQ(kem_group->iana_id, predicted_kem_group->iana_id);
        }

        EXPECT_SUCCESS(s2n_test_tls13_pq_handshake(client_policy, server_policy, kem_group, curve, hrr_expected, len_prefix_expected));
    }

    END_TEST();
}
