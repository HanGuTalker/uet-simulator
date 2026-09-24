# MRC model scope

## Project status

The MRC adapter is available as an initial functional comparison milestone. It implements the
standard Write data-path layout, packet spreading, out-of-order receive placement and basic
reliable recovery. It is not yet the completed MRC comparison model: reliability SACK/NACK and
NSCC are the next milestone.

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
- Cumulative semantic Transport ACK packets and a sender byte flight window.
- Per-packet retransmission timers with bounded exponential backoff and retransmission on another
  entropy path.
- Standard structured workload outputs and common transport counters.

The common `WRITE` and `MESSAGE` requests do not currently expose a registered memory address, so
the workload message identifier is used as a deterministic synthetic base virtual address. This is
a simulation mapping and is not presented as a verbs API.

## Deferred MRC functions

- Reliability SACK, NACK and probe packets, including SETH and NETH serialization.
- SACK-clocked NSCC with independent RTT and ECN congestion signals.
- Write-with-Immediate and receiver notification semantics.
- Endpoint discovery request/response and negotiated connection parameters.
- Packet trimming and path-health feedback.
- Memory registration/protection enforcement and complete verbs queue semantics.

Until these functions are added, results from this adapter characterize the implemented MRC core,
not full MRC 1.0 conformance or final MRC congestion-control performance.

## Targeted verification

The optimized ns-3.47 target was compiled and exercised at 800 Gbit/s without running the full
regression suite:

- two-node 1 MiB Write: 1/1 message completed in 71.512 us at 117.304 Gbit/s, with no
  retransmission, timeout or NACK; and
- four-node 256 KiB Write All-to-All: 12/12 messages completed, with 19.140 us mean latency and
  514.218 Gbit/s aggregate goodput, with no recovery event.

These runs are functional smoke checks. They are not tuned performance comparisons.
