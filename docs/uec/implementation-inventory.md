# UEC AI Base implementation inventory

This inventory freezes the implementation state before the common multi-transport framework is
introduced. Status describes code that exists in the frozen revision; historical test evidence is
listed separately and was not rerun while preparing this inventory.

| Area | Status | Primary implementation | Existing evidence | Boundary |
|---|---|---|---|---|
| PDS wire format | Implemented | `uet-pds-header.{h,cc}` | `UetWireHeaderLayoutTestCase` | Real serialized bytes; simulation-only routing metadata is not on wire |
| SES wire format | Implemented | `uet-ses-header.{h,cc}` | `UetWireHeaderLayoutTestCase` | Standard, medium, atomic extension and response formats in AI Base scope |
| CRC32C model | Implemented | `uet-crc-trailer.{h,cc}`, `uet-udp-transport.{h,cc}` | CRC tests and IPv4/IPv6 integration cases | Covers UET UDP payload, not mutable IP fields |
| PDC lifecycle | Implemented | `uet-pdc.{h,cc}`, `uet-endpoint.{h,cc}` | lifecycle and SYN retry cases | Event-level endpoint model, not a NIC pipeline |
| RUD reliability | Implemented | `uet-endpoint.{h,cc}` | loss, ACK loss, timeout, NACK, reorder and PSN-wrap cases | CACK/SACK, duplicate suppression and selective recovery are modeled |
| ROD reliability | Implemented | `uet-endpoint.{h,cc}` | ordering and PSN-wrap cases | Single-path ordered delivery with out-of-order drop and Go-Back-N recovery |
| UUD | Implemented | `uet-endpoint.{h,cc}` | `UetUudDatagramTestCase` | Single-packet, connectionless and without reliability feedback |
| SES operations | Implemented for mandatory profile | `uet-ses-engine.{h,cc}` | SES engine and end-to-end cases | Optional READ, fetching atomics, rendezvous and tagged operations excluded |
| NSCC | Implemented | `uet-nscc.{h,cc}`, endpoint pacing/gating | algorithm and endpoint integration cases | Per-PDC controller with delay, ECN and loss inputs |
| Trimming receive behavior | Implemented | `uet-endpoint.{h,cc}` | trim recovery and invalid-control cases | Switch trimming generation is an experiment/network function |
| UDP/IP integration | Implemented | `uet-udp-transport.{h,cc}` | IPv4 and IPv6 integration cases | IP/UDP are owned by ns-3 |
| Performance statistics | Implemented | `uet-ai-workload-example.cc` | existing CSV/JSON artifacts | Payload, wire-accounting, latency, queue, cwnd and collective outputs |
| Traffic patterns | Implemented | `uet-ai-workload-example.cc` | Single, Incast, All-to-All and Ring AllReduce artifacts | Ring model excludes GPU reduction and kernel-launch time |
| Endpoint multipath selection | **Plumbing only** | `uet-header`, `uet-simulation-tag`, endpoint trace surface | trace-contract case only | Outgoing data currently assigns `pathId=0`; no production path scheduler or per-path NSCC state |

## AI Base requirement state

The machine-readable checklist in `requirements/ai-base.yaml` contains 20 mandatory rows marked
`verified`. Their evidence is summarized in `ai-base-coverage.md`. In this repository, “verified”
means that the behavior passed the ns-3 model's historical focused tests; it is not UEC
certification.

The multipath plumbing row above is deliberately separated from those 20 checklist rows. It must
not be presented as a completed multipath implementation in future protocol comparisons. The common
transport framework should introduce a real path-manager interface before veRoCE, MRC, Falcon or
MetaRoCE multipath results are compared.

## Frozen observability surface

The endpoint exposes packet transmit/receive, PDC state, ACK, NACK, timeout, retransmission,
congestion-window, ECN, selected-path, trimming, reorder-depth and message-completion traces. The
workload driver additionally records per-message results, queue occupancy/marks, time-binned
throughput, per-collective completion and aggregate CSV/JSON summaries.

New transports should feed the same protocol-neutral statistics where meanings align. Metrics that
do not have identical semantics across protocols must carry a protocol-specific name and definition.
