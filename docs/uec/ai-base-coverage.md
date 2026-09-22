# AI Base mandatory-profile coverage

Baseline: UEC 1.0.3, AI Base column of the UEC Transport Matrix and Checklist revision 0.8.
The 20 mandatory rows are implemented and have executable evidence. This is simulation-profile
coverage, not UEC certification or a claim that the ns-3 model is a production NIC.

| Matrix ID | Implemented behavior | Primary evidence |
|---|---|---|
| SES-1 | NO_OP and status response | `UetSesEngineTestCase` |
| SES-2 | SEND, fragmentation through at least one payload MTU, message completion | `UetRudMixedLossStressTestCase` |
| SES-3 | connectionless DATAGRAM SEND/medium header | `UetUudDatagramTestCase` |
| SES-6 | authorized target-memory WRITE | `UetSesEndToEndTestCase` |
| SES-7 | WRITE immediate data and response | `UetSesEndToEndTestCase` |
| SES-9 | non-fetching WRITE/SUM/OR/AND/XOR/MIN/MAX atomics and datatypes | `UetSesEngineTestCase` |
| SES-15 | relative job/PID/resource-index addressing | `UetSesEngineTestCase` |
| SES-16 | absolute registered-buffer addressing | `UetSesEngineTestCase` |
| SES-19 | generation, job, PID, initiator and access authorization | `UetSesEngineTestCase` |
| SES-21 | memory-key validation | `UetSesEngineTestCase` |
| SES-26 | 44-byte standard request wire format | `UetWireHeaderLayoutTestCase` |
| SES-29 | Deferrable Send decoded and executed with Send semantics | `UetSesEngineTestCase` |
| SES-33 | 4-byte atomic extension wire format | `UetWireHeaderLayoutTestCase` |
| SES-35 | 16-byte semantic response carried in ACK_CC | `UetWireHeaderLayoutTestCase`, `UetSesEndToEndTestCase` |
| SES-38 | Delivery Complete/GO request bit and post-operation completion | `UetSesEngineTestCase`, `UetSesEndToEndTestCase` |
| PDS-1 | RUD, CACK/SACK, duplicate suppression, timeout/NACK recovery, PSN wrap | RUD recovery and stress tests |
| PDS-2 | single-path ROD, ordered delivery and Go-Back-N recovery | `UetRodOrderingTestCase`, ROD PSN-wrap test |
| PDS-4 | single-packet UUD without PDC/reliability feedback | `UetUudDatagramTestCase` |
| PDS-5 | endpoint trim detection, UET_TRIMMED NACK and recovery; trimmed SYN/control rejection | trim recovery and trim-rule tests |
| CMS-1 | NSCC ACK state, delay/ECN/loss response, congestion window, pacing and send gating | `UetNsccAlgorithmTestCase`, endpoint integration tests |

## Transport and network integration

The profile uses UEC PDS/SES bytes inside UDP over IPv4 or IPv6. `UetUdpTransport` enforces path
MTU, sets ECN-capable transport marking, imports received CE state, appends/verifies a CRC32C model,
and maps ns-3 endpoint/path metadata through non-wire packet tags. IPv4 and IPv6 point-to-point
integration tests exercise PDC establishment, data, semantic response and acknowledgement traffic.

`uet-ai-workload-example` supplies parameterized four-node incast and all-to-all runs over a real
ns-3 CSMA fabric. The lower-level direct-callback tests deliberately remain available for precise,
deterministic loss, reordering and trim injection.

## Fidelity boundary

The CRC model covers the UET UDP payload visible to this module; ns-3's IP layer owns the mutable IP
header and UDP checksum. Host memory/DMA and NIC pipelines are event-level abstractions. Optional
AI Base capabilities (RUDI, RCCC, transport security, READ, fetching atomics, rendezvous and tagged
operations) are intentionally excluded. These boundaries do not remove any mandatory row above.
