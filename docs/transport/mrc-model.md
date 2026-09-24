# MRC model scope

## Project status

The MRC adapter is available as a functional comparison subset. It implements the standard Write
data path, packet spreading, out-of-order receive placement, the MRC reliability control path and
SACK-clocked NSCC. It is not a full MRC endpoint because Write-with-Immediate, endpoint operations,
trimming and the complete verbs/resource model remain deferred.

## Normative basis

The wire model follows *OCP MRC Specification 1.0*, dated 21 March 2026. MRC uses UDP destination
port 4971 in this model. All multi-byte protocol fields are serialized in network byte order.
RoCE-compatible BTH, RETH, AETH and invariant-CRC primitives are shared from the `roce` module;
MRC-specific headers and state remain in the `mrc` module.

## Implemented core

- RDMA Write packetization. The common `MESSAGE` operation maps to MRC Write.
- MRC Write First, Middle, Last and Only opcodes.
- The four-byte Message Extended Transport Header (METH), carrying RQMSN and MSN.
- The four-byte requester timestamp header (TSETH), including timestamp-resolution and format-type
  fields, and the BTH indication that TSETH is present.
- RETH on every Write packet. The virtual address advances by payload MTU while the remote key and
  whole-operation DMA length remain constant.
- A 24-bit PSN space and 16-bit message sequence space per connection.
- Packet spreading over four independently bound UDP source ports by default. Round-robin path
  selection is explicit and exported through the common `PathSelected` trace.
- Responder out-of-order PSN tracking and direct-placement accounting by RETH virtual address.
- Receiver completion only after every packet order through the Last/Only packet has arrived.
- Cumulative semantic Transport ACK packets, kept logically independent from Reliability SACKs.
- Serialized 28-byte SETH plus eight-byte CC_STATE, including the cumulative PSN, triggering PSN
  offset, reflected entropy, QP/PDC identifiers, maximum PSN range, 64-bit selective bitmap,
  reflected timestamp, out-of-order count and cumulative received-byte clock.
- Serialized 20-byte NETH with the standard MRC reason codes and 16-byte PETH reliability probes.
- SACK bitmap processing, cumulative and selective resource release, a one-BDP reordering guard,
  and at-most-once selective fast retransmission of inferred holes.
- Retriable NACK processing, reliability-probe request/response, per-packet retransmission timers
  and retransmission on a different entropy path.
- Per-QP, sender-side, SACK-clocked NSCC. Reflected 128 ns timestamps provide RTT samples, SETH `m`
  carries ECN feedback, and the controller applies fair additive increase, bounded multiplicative
  decrease and a one-nominal-packet minimum window.
- Standard structured workload outputs and common transport counters.

The common `WRITE` and `MESSAGE` requests do not currently expose a registered memory address, so
the workload message identifier is used as a deterministic synthetic base virtual address. This is
a simulation mapping and is not presented as a verbs API.

## Deferred MRC functions

- Write-with-Immediate and receiver notification semantics.
- Endpoint discovery request/response and negotiated connection parameters.
- Packet trimming and path-health feedback.
- Memory registration/protection enforcement and complete verbs queue semantics.
- Full QP error-state transitions for non-retriable NACKs and retry exhaustion.

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

These runs are functional smoke checks. They are not tuned performance comparisons.
