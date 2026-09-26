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

The congestion metadata area remains opaque at this stage so the wire image is preserved without
prematurely assigning Swift/RUE/CSIG behavior. Resync, the transaction and packet-delivery state
machines, Swift/RUE congestion control, PLB, PSP/ESP overhead modeling and the common transport
adapter are subsequent phases. Until the adapter is registered, `--transport=falcon` remains
intentionally unavailable to workload experiments.

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
stack. Swift/RUE congestion control, PLB, Resync, Pull transactions and PSP/ESP overhead remain
deferred and are not claimed by this comparison subset.
