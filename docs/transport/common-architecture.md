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
| `MessageComplete` | workload message completion and latency |

A protocol must not emit a common trace for a semantically different event merely to populate a
chart. Protocol-specific metrics may be added alongside these traces with explicit definitions.

## UEC migration

`UetTransportAdapter` wraps the existing `UetEndpoint` and `UetUdpTransport`. It translates common
configuration, connection and message requests into the existing RUD data path and forwards UEC
trace events to the common trace surface. The adapter reports packet spraying and per-path
congestion control as unsupported because the frozen UEC sender still assigns `pathId=0`.

The workload entry point now accepts `--transport=uec`. Names for veRoCE, MRC, Falcon, MetaRoCE and
RoCEv2 are parsed now. Until the corresponding adapter is registered, selecting one fails explicitly
and never falls back to UEC.

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

The optimized common module, UEC adapter and migrated workload entry point compile successfully.
A two-node, one-message UEC smoke run completed one of one messages and emitted a JSON summary with
`protocol: uec`. A request for `transport=mrc` was recognized and rejected because no MRC adapter is
registered yet. No performance or full regression matrix was run during this phase.
