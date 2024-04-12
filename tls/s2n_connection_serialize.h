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

#include <stdint.h>

#include "tls/s2n_connection.h"

#pragma once

#define S2N_SERIALIZED_CONN_FIXED_SIZE (8 + S2N_TLS_PROTOCOL_VERSION_LEN + S2N_TLS_CIPHER_SUITE_LEN \
        + S2N_TLS_SEQUENCE_NUM_LEN + S2N_TLS_SEQUENCE_NUM_LEN + 2)
#define S2N_SERIALIZED_CONN_TLS12_SIZE (S2N_SERIALIZED_CONN_FIXED_SIZE + S2N_TLS_SECRET_LEN \
        + S2N_TLS_RANDOM_DATA_LEN + S2N_TLS_RANDOM_DATA_LEN)

/* APIs that will be moved to s2n.h when the connection serialize feature is released */
int s2n_connection_serialization_length(struct s2n_connection *conn, uint32_t *length);
int s2n_connection_serialize(struct s2n_connection *conn, uint8_t *buffer, uint32_t buffer_length);
int s2n_connection_deserialize(struct s2n_connection *conn, uint8_t *buffer, uint32_t buffer_length);