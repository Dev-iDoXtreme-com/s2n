# Post Quantum (PQ) Support

s2n-tls supports both post-quantum key exchange and post-quantum authentication for TLS1.3.

## Key Exchange: ML-KEM / Kyber

Currently, only [ML-KEM](https://csrc.nist.gov/pubs/fips/203) / [Kyber](https://pq-crystals.org/kyber/) are supported for post-quantum key exchange. "ML-KEM" is the name given to the NIST standardized version of Kyber.

Specifically, s2n-tls supports hybrid key exchange. PQ hybrid key exchange involves performing both classic ECDH key exchange and post-quantum key exchange, then combining the two resultant secrets. This strategy combines the high assurance of the classical key exchange algorithms with the quantum-resistance of the new post-quantum key exchange algorithms. If one of the two algorithms is compromised, either because advances in quantum computing make the classic algorithms insecure or because cryptographers find a flaw in the relatively new post-quantum algorithms, the secret is still secure. Hybrid post-quantum key exchange is more secure than standard key exchange, but is slower and requires more processing and more network bandwidth.

Careful: An s2n-tls server that enables post-quantum cryptography will mandate post-quantum key exchange with any client advertising post-quantum algorithms. This can result in a retry and an extra round trip if the client does not initially send a post-quantum key share. The rational behind this behavior is that post-quantum users prioritize security over the potential cost of an extra round trip.

## Authentication: ML-DSA

Currently, only [ML-DSA](https://csrc.nist.gov/pubs/fips/204) is supported for post-quantum authentication.

In order to use ML-DSA, you must configure s2n-tls to use an ML-DSA certificate, just as you would configure an RSA or ECDSA certificate. See [certificates](./ch09-certificates.md).

## Requirements

### AWS-LC

s2n-tls must be built with aws-lc to use post-quantum algorithms. See the [s2n-tls build documentation](https://github.com/aws/s2n-tls/blob/main/docs/BUILD.md#building-with-a-specific-libcrypto) for how to build with aws-lc. For ML-DSA, you will need to use a version of AWS-LC >= v1.50.0 (API version 33).

If you're unsure what cryptography library s2n-tls is built against, trying running s2nd or s2nc:
```
> s2nd localhost 8000
libcrypto: AWS-LC
Listening on localhost:8000
```

### Security Policy

Post-quantum algorithms are enabled by configuring a security policy (see [Security Policies](./ch06-security-policies.md)) that supports post-quantum algorithms. 

"default_pq" is the equivalent of "default_tls13", but with PQ support. Like the other default policies, "default_pq" may change as a result of library updates. The fixed, numbered equivalent of "default_pq" is currently "20250721". For previous defaults, see the "Default Policy History" section below.

Other available PQ policies are compared in the tables below.

### Chart: Security Policy Version To PQ Hybrid Key Exchange Methods (ML-KEM)

|        Version        | x25519+mlkem768 | secp256r1+mlkem768 | secp384r1+mlkem1024 |
|-----------------------|-----------------|--------------------|---------------------|
| default_pq / 20250721 |        X        |          X         |          X          |
| 20250512              |        X        |          X         |                     |

### Chart: Security Policy Version To PQ Hybrid Key Exchange Methods (Kyber)

|        Version        | secp256r1+kyber768 | x25519+kyber768 | secp384r1+kyber768 | secp521r1+kyber1024 | secp256r1+kyber512 | x25519+kyber512 |
|-----------------------|--------------------|-----------------|--------------------|---------------------|--------------------|-----------------|
| 20240730              |          X         |         X       |         X          |          X          |         X          |        X        |
| PQ-TLS-1-2-2023-12-15 |          X         |                 |         X          |          X          |         X          |                 |
| PQ-TLS-1-2-2023-12-14 |          X         |                 |         X          |          X          |         X          |                 |
| PQ-TLS-1-2-2023-12-13 |          X         |                 |         X          |          X          |         X          |                 |
| PQ-TLS-1-2-2023-10-10 |          X         |         X       |         X          |          X          |         X          |        X        |
| PQ-TLS-1-2-2023-10-09 |          X         |         X       |         X          |          X          |         X          |        X        |
| PQ-TLS-1-2-2023-10-08 |          X         |         X       |         X          |          X          |         X          |        X        |
| PQ-TLS-1-2-2023-10-07 |          X         |         X       |         X          |          X          |         X          |        X        |
| PQ-TLS-1-3-2023-06-01 |          X         |         X       |         X          |          X          |         X          |        X        |

### Chart: Security Policy Version To Signature Schemes

|        Version        | ML-DSA | ECDSA | RSA | RSA-PSS | Legacy SHA1 |
|-----------------------|--------|-------|-----|---------|-------------|
| default_pq / 20250721 |   X    |   X   |  X  |    X    |             |
| 20250512              |   X    |   X   |  X  |    X    |             |
| 20240730              |        |   X   |  X  |    X    |             |
| PQ-TLS-1-2-2023-12-15 |        |   X   |  X  |    X    |             |
| PQ-TLS-1-2-2023-12-14 |        |   X   |  X  |    X    |             |
| PQ-TLS-1-2-2023-12-13 |        |   X   |  X  |    X    |             |
| PQ-TLS-1-2-2023-10-10 |        |   X   |  X  |    X    |      X      |
| PQ-TLS-1-2-2023-10-09 |        |   X   |  X  |    X    |      X      |
| PQ-TLS-1-2-2023-10-08 |        |   X   |  X  |    X    |      X      |
| PQ-TLS-1-2-2023-10-07 |        |   X   |  X  |    X    |      X      |
| PQ-TLS-1-3-2023-06-01 |        |   X   |  X  |    X    |      X      |

### Chart: Security Policy Version To Classic Key Exchange

If the peer doesn't support a PQ hybrid key exchange method, s2n-tls will fall back to a classical option.

|        Version        | secp256r1 | x25519 | secp384r1 | secp521r1 | DHE | RSA |
|-----------------------|-----------|--------|-----------|-----------|-----|-----|
| default_pq / 20250721 |     X     |   X    |     X     |     X     |     |     |
| 20250512              |     X     |   X    |     X     |     X     |     |     |
| 20240730              |     X     |   X    |     X     |     X     |     |     |
| PQ-TLS-1-2-2023-12-15 |     X     |        |     X     |     X     |  X  |     |
| PQ-TLS-1-2-2023-12-14 |     X     |        |     X     |     X     |     |     |
| PQ-TLS-1-2-2023-12-13 |     X     |        |     X     |     X     |     |  X  |
| PQ-TLS-1-2-2023-10-10 |     X     |   X    |     X     |           |  X  |  X  |
| PQ-TLS-1-2-2023-10-09 |     X     |   X    |     X     |           |  X  |     |
| PQ-TLS-1-2-2023-10-08 |     X     |   X    |     X     |           |  X  |  X  |
| PQ-TLS-1-2-2023-10-07 |     X     |   X    |     X     |           |     |  X  |
| PQ-TLS-1-3-2023-06-01 |     X     |        |     X     |     X     |  X  |  X  |

### Chart: Security Policy Version To Ciphers

|        Version        | AES-CBC | AES-GCM | CHACHAPOLY | 3DES |
|-----------------------|---------|---------|------------|------|
| default_pq / 20250721 |    X    |    X    |     X      |      |
| 20250512              |    X    |    X    |     X      |      |
| 20240730              |    X    |    X    |     X      |      |
| PQ-TLS-1-2-2023-12-15 |    X    |    X    |            |      |
| PQ-TLS-1-2-2023-12-14 |    X    |    X    |            |      |
| PQ-TLS-1-2-2023-12-13 |    X    |    X    |            |      |
| PQ-TLS-1-2-2023-10-10 |    X    |    X    |     X*     |  X   |
| PQ-TLS-1-2-2023-10-09 |    X    |    X    |     X*     |  X   |
| PQ-TLS-1-2-2023-10-08 |    X    |    X    |     X*     |  X   |
| PQ-TLS-1-2-2023-10-07 |    X    |    X    |     X*     |      |
| PQ-TLS-1-3-2023-06-01 |    X    |    X    |     X*     |  X   |
\* only for TLS1.3

### Chart: Security Policy Version To TLS Protocol Version

|        Version        | 1.2 | 1.3 |
|-----------------------|-----|-----|
| default_pq / 20250721 |  X  |  X  |
| 20250512              |  X  |  X  |
| 20240730              |  X  |  X  |
| PQ-TLS-1-2-2023-12-15 |  X  |  X  |
| PQ-TLS-1-2-2023-12-14 |  X  |  X  |
| PQ-TLS-1-2-2023-12-13 |  X  |  X  |
| PQ-TLS-1-2-2023-10-10 |  X  |  X  |
| PQ-TLS-1-2-2023-10-09 |  X  |  X  |
| PQ-TLS-1-2-2023-10-08 |  X  |  X  |
| PQ-TLS-1-2-2023-10-07 |  X  |  X  |
| PQ-TLS-1-3-2023-06-01 |  X  |  X  |

#### Default Policy History
|  Version   | "default_pq" |
|------------|--------------|
|  v1.5.23   |   20250721   |
|  v1.5.19   |   20250512   |
|  v1.5.0    |   20240730   |

## Visibility
Call `s2n_connection_get_kem_group_name` to determine if a TLS handshake negotiated PQ key exchange.