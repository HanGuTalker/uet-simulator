# veRoCE model scope

## Normative basis

The model follows *ByteDance veRoCE Transport Protocol*, version 2.0.4, dated 18 December
2025. The implementation uses the specified big-endian field order and the IANA-assigned UDP
destination port 4794. RoCEv2-compatible BTH, RETH, AETH, CNP and invariant-CRC primitives are
shared from the `roce` module; veRoCE extensions remain in the `veroce` module.

## Implemented P2 core

- RC Write and Send packetization. The common `MESSAGE` operation maps to RDMA Write.
- BTH retransmission flag and veRoCE opcodes.
- Serialized MSNETH, POETH, RQETH, SACKETH and RTT request/response header formats.
- MSN per message, PSN per packet and PO per message fragment.
- Out-of-order receive with direct payload placement accounting and a configurable PSN bitmap.
- Cumulative ACK plus 128-bit Lazy SACK once receive OOOD exceeds a configurable threshold.
- RxtPSN-style suppression of overlapping SACK-triggered retransmissions, plus RTO recovery.
- Sender packet spreading across distinct UDP source ports, preserving the selected path ID in
  simulation metadata and common traces.
- Path-specific ECN-to-CNP feedback and a rate-based FCC context per path. The default controller
  uses DCQCN-style reduction/recovery as one legal FCC algorithm; FCC itself does not mandate a
  specific rate-control equation.
- Receiver-side message completion after every PO from zero through the Last/Only packet is
  present, independent of packet arrival order.

## Deliberate limitations

This is a runnable P2 data-plane core, not a claim of full veRoCE 2.0.4 conformance. RDMA Read,
Atomic, Write-with-Immediate, packet trimming, slow-path signaling, RTT probe scheduling, shared
receive queues, CM/profile negotiation and P0/P1/P3 behavior remain separate milestones. The RTT
wire header is implemented but probes are not yet scheduled by the adapter. The simulated ICRC
protects the transport image; ns-3 owns the outer IP/UDP serialization, so mutable outer fields are
not copied into the model's CRC calculation.

The comparison framework reports only implemented capabilities. In particular,
`endpointTrimming=false`, `reliableOrdered=false` and `jobScheduling=false` until those behaviors
exist and have targeted tests.

## Targeted verification

The optimized ns-3.47 target was compiled and exercised at 800 Gbit/s without running the full
regression suite:

- two-node 4 KiB single flow: 1/1 messages completed, 2.084 us latency, no recovery event;
- four-node 1 MiB Incast with RED/ECN: 3/3 completed, 351.331 Gbit/s aggregate goodput, 34 queue
  marks, 12 endpoint ECN events and no queue drop; and
- four-node 256 KiB lossy Incast with a 32 KiB hard queue limit: 3/3 completed despite 32 dropped
  packets, using 42 retransmissions, of which 27 were initiated before timeout by SACK recovery.

These are functional smoke results, not a protocol-performance comparison or tuning claim.
