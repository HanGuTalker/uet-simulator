# Simulation scope and fidelity

## Modeled in the AI Base delivery

- Packet-level endpoints and real ns-3 links/queues.
- Protocol-visible UET headers, identifiers, timers, acknowledgments, and errors.
- FEP and PDC lifecycle behavior.
- RUD, ROD, and UUD delivery modes.
- NSCC signals and sender-side control behavior.
- ECN, loss, reordering, path metadata/trace hooks, and optional switch trimming.
- AI communication workloads: parameterized incast and all-to-all traffic.
- IPv4/UDP and IPv6/UDP encapsulation, ECN transport markings, path-MTU rejection, and CRC32C.

## Parameterized abstractions

- NIC processing delay.
- Host memory and DMA service delay.
- Switch pipeline delay.
- Link data rate, propagation delay, and serialization delay.
- Queue capacity and ECN thresholds.

The defaults are engineering assumptions until calibrated against a hardware or reference
implementation. Results must identify assumed versus measured inputs.

## Optional or lower-layer behavior not modeled

- PHY bit errors, FEC internals, SerDes behavior, and signal integrity.
- Cycle-accurate NIC, PCIe, memory-controller, or switch pipelines.
- RUDI, RCCC, TSS, READ, tagged operations, fetching atomics, rendezvous, and in-network
  collectives.
- Full libfabric or framework integration.
- Real-time emulation or hardware-in-the-loop behavior.
- Endpoint-controlled multipath scheduling and per-path NSCC state. The current sender assigns
  `pathId=0`; the existing path field, enable flag and trace source are integration plumbing only.

## Accuracy statement

The project validates modeled protocol behavior and compares performance under controlled
assumptions. It must not claim formal UEC certification or interoperability compliance.
