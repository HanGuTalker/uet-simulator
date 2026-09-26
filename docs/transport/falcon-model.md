# Falcon model scope

## Normative basis

The implementation follows the *OCP Falcon Networking Transport Specification*, revision 1.1.0,
dated 22 April 2026. The public Isekai simulator is used as a behavior reference, while the OCP
document remains the normative source for wire fields and state-machine requirements.

## Phases P0-P1: wire foundation

The initial Falcon module provides:

- the specification-defined 24-byte Falcon base header;
- exact Version, Destination CID, Destination Function, Protocol Type, Packet Type, Ack Req,
  receiver data/request window base PSN, packet PSN and request sequence number fields;
- the four-byte Push Data suffix with its reserved field forced to zero;
- the eight-byte Pull Request suffix with both reserved fields forced to zero;
- the 32-byte Base ACK (BACK), including cumulative data/request window bases, timestamps and an
  opaque lossless representation of the congestion metadata region;
- the 72-byte Extended ACK (EACK), including 128-bit data ACK and receive bitmaps plus the 64-bit
  request bitmap;
- the 40-byte NACK, including the NACK PSN, reason, RNR timeout, window selector and ULP reason;
- byte-level serialization and round-trip unit tests in network byte order.

The congestion metadata area is preserved as an opaque wire field. This prevents the simulator
from inventing a hardware-specific RUE metadata encoding while later phases implement the
observable congestion-control behavior.

## Phase P2: reliability state

`FalconReliabilityManager` implements the protocol's independent data and request PSN spaces. It
tracks transmitted packets, rejects duplicates and packets beyond the representable ACK window,
advances cumulative data/request window bases, generates BACK/EACK state and consumes BACK, EACK
and NACK feedback. Data-received and ULP-acknowledged states remain distinct: an EACK receive bit
suppresses ambiguity without retiring the sender's packet until the ACK bitmap confirms it.

PSN wraparound is deferred; the implementation asserts before local allocation wraps so
experiments cannot silently compare ambiguous sequence numbers.

## Phase P3: common transport adapter

`FalconTransportAdapter` registers Falcon with the common AI workload framework. MESSAGE, SEND and
WRITE requests using reliable-unordered delivery are fragmented into Push Data packets, paced at
the configured connection rate and admitted under the configured byte window. Receivers perform
out-of-order placement, return EACK state, issue NACK for packets outside the represented receive
window and complete a message only after all simulation fragments arrive. Senders release window
bytes from cumulative or selective ACKs and use exponential-backoff timeouts plus NACK-triggered
fast retransmission.

`FalconSimulationTag` carries experiment-only source, message and fragmentation context without
adding bytes to the Falcon wire image. The adapter currently models Falcon over an ns-3 UDP
substrate, as do the other comparison adapters; UDP/IP overhead is still included by the network
stack.

## Phase P4: Swift/RUE congestion-control subset

`FalconSwift` implements the connection-level behavior described by the specification's Rate
Update Engine chapter. It maintains independent fabric and NIC congestion windows (`fcwnd` and
`ncwnd`), samples RTT and fabric delay from the four packet/ACK timestamps, smooths both signals,
performs additive increase below the target delay and proportional multiplicative decrease above
it, and applies the once-per-RTT decrease guard. A window below one packet admits one outstanding
packet and adds the specification-defined pacing delay.

The adapter feeds EACK measurements to Swift, applies the effective minimum of `fcwnd`, `ncwnd`
and the configured byte cap to admission, and derives the retransmission timeout from smoothed RTT
with a configurable scalar and minimum. First and consecutive retransmissions reduce `fcwnd` as
specified; a resource-exhaustion NACK collapses `ncwnd`. Full-resolution ns-3 timestamps are kept
only in `FalconSimulationTag`, while the real BACK/EACK timestamp fields remain serialized in the
specified 131.072 ns unit.

This is a comparison-oriented Swift/RUE subset rather than a Falcon conformance claim. The OCP
specification deliberately leaves algorithm parameters programmable, so the simulator provides
explicit defaults through `FalconSwiftConfig`. Hardware RUE request/response queues, CSIG parsing,
PLB path migration, Resync, complete Pull transactions, PSP/ESP processing and resource-manager
limits remain deferred.

## Verification

Wire headers and reliability behavior retain their byte-level and state-machine tests. The Swift
suite deterministically covers additive increase, delay-based multiplicative decrease, the RTT
guard, retransmission collapse and sub-packet-window pacing. An adapter-level two-node test drops
the first data frame deliberately and verifies timeout retransmission and eventual message
completion.
