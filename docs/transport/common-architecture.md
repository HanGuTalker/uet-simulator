# Common AI transport architecture

## Goal

The `ai-transport` module provides the protocol-neutral boundary used to compare UEC, veRoCE, MRC,
Falcon, MetaRoCE and the RoCEv2/DCQCN baseline under identical ns-3 topologies and workloads. It
does not place protocol wire headers or reliability state in a shared base class; those details stay
inside each protocol module.

## Module boundary

```text
AI workload and collective scheduler
                 |
        AiTransportEndpoint
                 |
        AiTransportFactory
                 |
   +------+------+------+------+------+
   |      |      |      |      |      |
  UEC  veRoCE   MRC  Falcon MetaRoCE RoCEv2
 adapter adapter adapter adapter adapter adapter
   |      |      |      |      |      |
protocol-specific wire format, reliability, CC and network binding
```

The common module consists of protocol identifiers and parsing, endpoint/connection/request
configuration, capability discovery, common counters, a stable endpoint contract and a per-run
adapter factory.

## Common trace contract

Every adapter exposes the same trace names where the meaning can be normalized:

| Trace | Common meaning |
|---|---|
| `PacketTx` / `PacketRx` | protocol packet handed to or accepted from its wire binding |
| `PayloadRx` | application payload bytes accepted from a source endpoint |
| `Retransmission` | protocol packet selected for retransmission |
| `Timeout` | recovery timer expiry |
| `Nack` | negative acknowledgement or equivalent explicit recovery signal |
| `CongestionWindow` | sending-window change in bytes |
| `EcnReceived` | ECN or protocol-equivalent explicit congestion feedback |
| `PacketTrimmed` | trim event for protocols that support it |
| `PathSelected` | selected path/entropy identifier |
| `ReorderDepth` | receive reorder-depth change |
| `MessageComplete` | receiver-side workload delivery completion and latency |

A protocol must not emit a common trace for a semantically different event merely to populate a
chart. Protocol-specific metrics may be added alongside these traces with explicit definitions.

## UEC migration

`UetTransportAdapter` wraps the existing `UetEndpoint` and `UetUdpTransport`. It translates common
configuration, connection and message requests into the existing RUD data path and forwards UEC
trace events to the common trace surface. The adapter reports packet spraying and per-path
congestion control as unsupported because the frozen UEC sender still assigns `pathId=0`.

The workload entry point accepts `--transport=uec`, `--transport=rocev2`, `--transport=veroce` and
`--transport=mrc`.
Its `--operation=message|send|write|write-imm|read` switch selects the common verb; RDMA Read is
currently accepted only by the veRoCE adapter and Write-with-Immediate only by MRC.
Names for Falcon and MetaRoCE are parsed now. Until the corresponding adapter is registered,
selecting one fails explicitly and never falls back to another protocol.

## Adapter requirements

Each new transport implementation must:

1. derive its adapter from `AiTransportEndpoint`;
2. retain wire headers, state machines and congestion control in its own module;
3. declare capabilities from implemented behavior, not specification intent;
4. document how the benchmark `MESSAGE` operation maps to its protocol semantics;
5. provide common counters and traces;
6. register its adapter `TypeId` with `AiTransportFactory`;
7. add its canonical name to structured results; and
8. reject unsupported reliability modes and operations explicitly.

The implementation registry is recorded in `requirements/transport-catalog.json`.

## Current verification

The optimized common module and registered UEC, RoCEv2, veRoCE and MRC adapters compile
successfully. The RoCEv2 module is registered as the conventional RC/DCQCN baseline; its exact
scope and limitations are documented in `docs/transport/rocev2-model.md`.

The veRoCE module registers a P2-oriented data-plane adapter with a P3 Read path on UDP port 4794.
It implements actual veRoCE extension-header serialization, out-of-order DDP accounting, Lazy
SACK/selective recovery, UDP source-port packet spreading, path-wise FCC and independent reliable
Read response state. Its supported and deferred protocol functions are documented in
`docs/transport/veroce-model.md`.

The MRC module registers a comparison subset with the MRC 1.0 Write and Write-with-Immediate wire
layouts, packet spraying, out-of-order direct placement, ordered completion, independent semantic
ACK and Reliability SACK/NACK processing, reliability probes, selective recovery, endpoint
trimming recovery, retry-limit/non-retriable QP error transitions and SACK-clocked NSCC. Its
connectionless Endpoint Operations provide EV Probe RTT/reachability, Port Status Update state,
active-EV selection with `GOOD`, `DENIED`, `SKIP` and `ASSUMED_BAD` recovery states, and the
specification-required out-of-band QP attribute setup.
Its implemented and deferred functions are documented in `docs/transport/mrc-model.md`.

The Falcon module has entered phase P1 with the OCP Falcon 1.1 base, Push Data, Pull Request,
BACK, EACK and NACK wire layouts plus byte-level serialization tests. It is not registered with
the workload driver until the reliable packet-delivery adapter is available; see
`docs/transport/falcon-model.md`.
