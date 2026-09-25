# MRC model scope

## Project status

The MRC adapter is available as a functional comparison subset. It implements the standard Write
and Write-with-Immediate data paths, packet spreading, out-of-order receive placement, packet
trimming, the MRC reliability control path and SACK-clocked NSCC. It is not a full MRC endpoint
because negotiated discovery, the full endpoint-visibility state machine and the complete
verbs/resource model remain deferred.

## Normative basis

The wire model follows *OCP MRC Specification 1.0*, dated 21 March 2026. MRC uses UDP destination
port 4971 in this model. All multi-byte protocol fields are serialized in network byte order.
RoCE-compatible BTH, RETH, AETH and invariant-CRC primitives are shared from the `roce` module;
MRC-specific headers and state remain in the `mrc` module.

## Implemented core

- RDMA Write packetization. The common `MESSAGE` operation maps to MRC Write.
- MRC Write First, Middle, Last and Only opcodes, plus Write Last/Only with Immediate opcodes.
- The four-byte Message Extended Transport Header (METH), carrying MSN on every request and a
  separately allocated RQMSN for Write-with-Immediate operations.
- A four-byte Immediate Data field after RETH on Write Last/Only with Immediate packets. The common
  API maps the low 32 bits of `messageId` to Immediate Data because it has no explicit immediate
  operand.
- The four-byte requester timestamp header (TSETH), including timestamp-resolution and format-type
  fields, and the BTH indication that TSETH is present.
- RETH on every Write packet. The virtual address advances by payload MTU while the remote key and
  whole-operation DMA length remain constant.
- A 24-bit PSN space and 16-bit message sequence space per connection.
- Packet spreading over four independently bound UDP source ports by default. Round-robin path
  selection is explicit and exported through the common `PathSelected` trace.
- Responder out-of-order PSN tracking and direct-placement accounting by RETH virtual address.
- Receiver completion only after every packet order through the Last/Only packet has arrived, with
  completions released in MSN order. Immediate values are stashed until completion and bounded by
  the configurable `MaxWriteImmediateInflight` responder-QP limit.
- Cumulative semantic Transport ACK packets, kept logically independent from Reliability SACKs.
  ACK AETH packets advertise the mandatory no-credit value `0x1f`.
- Serialized 28-byte SETH plus eight-byte CC_STATE, including the cumulative PSN, triggering PSN
  offset, reflected entropy, QP/PDC identifiers, maximum PSN range, 64-bit selective bitmap,
  reflected timestamp, out-of-order count and cumulative received-byte clock.
- Serialized 20-byte NETH with the standard MRC reason codes and 16-byte PETH reliability probes.
- SACK bitmap processing, cumulative and selective resource release, a one-BDP reordering guard,
  and at-most-once selective fast retransmission of inferred holes.
- Retriable NACK processing, reliability-probe request/response, per-packet retransmission timers
  and retransmission on a different entropy path.
- Requestor QP transition to ERROR after retry exhaustion or a non-retriable unexpected-event
  NACK. WriteIMM stash exhaustion returns an AETH Invalid Request NAK and causes the remote
  requestor QP to enter ERROR instead of incorrectly treating the condition as retriable.
- DSCP-9 trimmed-packet recognition before payload parsing, a common `PacketTrimmed` trace,
  Reliability NACK with reason `TRIMMED`, and sender fast retransmission on a different path. The
  shared switched-fabric trim queue preserves the headers needed to identify the affected PSN.
- Connectionless Endpoint Operations using the 16-byte ERTH and 36-byte EETH layouts, reserved
  destination QP `0x2`, and a request-private identifier in BTH PSN[15:0]. EV Probe responses
  update per-peer/path reachability and RTT, while Port Status Update requests publish and retain
  the peer's 32-bit reachable-port mask. These best-effort exchanges consume no QP PSNs and are
  not retransmitted. A configurable response timeout marks an unresponsive EV path unreachable
  and releases request state.
- Per-QP EV state tracking for `GOOD`, `DENIED`, `SKIP` and `ASSUMED_BAD`. Data scheduling selects
  only `GOOD` EVs; ECN or trim feedback temporarily skips an EV, timeouts quarantine it, and
  periodic EV Probes restore a recovered path to `GOOD`. Operator-denied EVs remain inactive until
  explicitly re-enabled.
- Out-of-band QP attribute provisioning, as required by MRC instead of RDMA-CM. Explicit setup
  gates data submission on `NEGOTIATING`, `READY` and `ERROR` states and exchanges the responder
  WriteIMM limit, maximum PSN range, bidirectional Dynamic MPR support, directional Trim NACK
  support and directional service-time support. Invalid attributes and setup timeout transition the
  QP to `ERROR`; the common comparison API retains an implicit symmetric provisioning mode.
- Enforcement of the negotiated responder WriteIMM limit and maximum number of in-flight packets.
  Dynamic MPR is enabled only when both peers advertise support and then follows non-zero MPR
  updates in SACKs.
- Per-QP, sender-side, SACK-clocked NSCC. Reflected 128 ns timestamps provide RTT samples, SETH `m`
  carries ECN feedback, and the controller applies fair additive increase, bounded multiplicative
  decrease and a one-nominal-packet minimum window.
- Standard structured workload outputs and common transport counters.

The common `WRITE` and `MESSAGE` requests do not currently expose a registered memory address, so
the workload message identifier is used as a deterministic synthetic base virtual address. This is
a simulation mapping and is not presented as a verbs API.

## Deferred MRC functions

- Controller-driven endpoint discovery and the complete `libmrc`/`mrc_ctl` programming API.
- Structured EV and SRv6 entropy formats. The current endpoint response reflects timestamps
  without service-time compensation. Path-health state is shared only through the simulator API,
  not the complete MRC Controller API.
- Memory registration/protection enforcement and complete verbs queue semantics.
- Full verbs-driven QP lifecycle, work-completion syndromes and the remaining RC memory/protection
  error paths.

Until these functions are added, results from this adapter characterize the implemented MRC core,
not full MRC 1.0 conformance or final MRC congestion-control performance.

## Targeted verification

The optimized ns-3.47 target was compiled and exercised at 800 Gbit/s without running the full
regression suite:

- two-node 1 MiB Write: 1/1 message completed in 71.512 us at 117.304 Gbit/s, with no
  retransmission, timeout or NACK; and
- four-node 256 KiB Write All-to-All: 12/12 messages completed, with 19.140 us mean latency and
  514.218 Gbit/s aggregate goodput, with no recovery event.
- two-node 1 MiB Write with PSN 4 deliberately dropped: 1/1 completed using one SACK-triggered fast
  retransmission and zero timeout;
- four-node 1 MiB Incast with an intentionally aggressive 512 ns target queue delay: 3/3 completed,
  171 ECN/SACK signals drove the sender windows from 630,000 bytes down to the 4,136-byte
  one-packet floor and then through additive recovery. Aggregate goodput was 257.119 Gbit/s.
- two-node 1 MiB Write with the final PSN deliberately dropped: 1/1 completed after one RTO, one
  PETH probe and one retransmission; the probe response added one SETH without corrupting state.
- two-node four-message 256 KiB Write-with-Immediate: 4/4 completed in MSN order, with 18.428 us
  mean latency, 106.959 Gbit/s aggregate goodput and no recovery event; and
- four-node 1 MiB Incast with a 32 KiB trim queue: 3/3 completed while 25 trimmed packets generated
  25 `TRIMMED` NACKs and 25 fast retransmissions, with zero timeout and zero queue drop. Aggregate
  goodput was 58.077 Gbit/s and mean latency was 152.310 us.
- two-node 800 Gbit/s Endpoint Operations exchange: EV Probe request/response produced a positive
  RTT sample, Port Status Update reflected a `0xa5` port mask into peer state, and a subsequent
  unanswered EV Probe moved the EV to `ASSUMED_BAD`. After the responder returned, the periodic
  recovery probe restored reachability and the EV's `GOOD` state without consuming connection
  PSNs.
- explicit out-of-band setup rejected pre-`READY` data, accepted and retained valid directional
  attributes, rejected an invalid zero MPR, and transitioned an unconfigured QP to `ERROR` on its
  setup timeout.

These runs are functional smoke checks. They are not tuned performance comparisons.
