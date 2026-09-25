# Falcon model scope

## Normative basis

The implementation follows the *OCP Falcon Networking Transport Specification*, revision 1.1.0,
dated 22 April 2026. The public Isekai simulator is used as a behavior reference, while the OCP
document remains the normative source for wire fields and state-machine requirements.

## Phase P0: wire foundation

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
