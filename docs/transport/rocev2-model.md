# RoCEv2/DCQCN baseline model

## Scope

The `roce` ns-3 module supplies the conventional baseline used before adding veRoCE extensions. It
uses IPv4/UDP port 4791 and serializes protocol bytes rather than representing headers only as
metadata. `MESSAGE` maps to RC Send; the common `WRITE` operation maps to RC RDMA Write.

Implemented wire structures are the 12-byte Base Transport Header (BTH), 16-byte RDMA Extended
Transport Header (RETH), 4-byte ACK Extended Transport Header (AETH), 16-byte CNP payload, and a
4-byte invariant CRC trailer. Endpoint identifiers, full message identifiers, and workload byte
counts are carried only in an ns-3 packet tag and do not affect the modeled wire size.

## Reliability

Each RC connection owns a 24-bit packet sequence space, a byte window, retransmission timers and
message completion state. The receiver accepts the next expected PSN, sends a cumulative ACK, and
sends a sequence NAK for a gap. A NAK retransmits outstanding packets from the missing PSN (go-back
N); timeout recovery uses bounded exponential backoff. This deliberately does not claim selective
ACK or out-of-order placement.

## Congestion control

Data packets are ECN-capable. A receiver that observes CE sends a rate-limited CNP to the source.
The source updates the DCQCN alpha estimate, reduces its per-QP rate, performs five fast-recovery
cycles and then additive rate recovery. The current model is rate based; the common congestion
window remains a reliability flight limit and is not mislabeled as the DCQCN rate.

PFC is not silently assumed by the endpoint. Queue size, ECN marking and packet drops remain fabric
experiment settings. Consequently the same transport can be evaluated with a lossless/PFC proxy
configuration or in a lossy fabric, where RC go-back-N recovery is visible.

## Current limitations

- IPv4 is implemented; IPv6 global routing headers are not yet modeled.
- Path selection is a single path (`pathId=0`), matching conventional per-QP ECMP behavior.
- READ, atomic operations, immediate data, memory registration and verbs queue semantics are out of
  scope for the current AI workload comparison.
- CNP generation follows CE observations, but switch-side PFC pause frames are not modeled inside
  the transport.

The packet layout follows the public IBTA RoCEv2 description and InfiniBand transport header
layout. DCQCN state and default timing follow the algorithm described by Zhu et al., SIGCOMM 2015;
all simulator-specific simplifications are listed above rather than presented as interoperable NIC
behavior.
