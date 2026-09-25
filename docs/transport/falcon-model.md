# Falcon model scope

## Normative basis

The implementation follows the *OCP Falcon Transport Protocol Specification*, revision 1.0,
dated 4 April 2024. The public Isekai simulator is used as a behavior reference, while the OCP
document remains the normative source for wire fields and state-machine requirements.

## Phase P0: wire foundation

The initial Falcon module provides:

- the specification-defined 24-byte Falcon base header;
- exact Version, Destination CID, Destination Function, Protocol Type, Packet Type, Ack Req,
  receiver data/request window base PSN, packet PSN and request sequence number fields;
- the two-byte Push Data request-length suffix;
- the six-byte Pull Request suffix with its reserved field forced to zero; and
- byte-level serialization and round-trip unit tests in network byte order.

ACK, EACK, NACK and Resync layouts, the transaction and packet-delivery state machines, Swift/RUE
congestion control, PLB, PSP/ESP overhead modeling and the common transport adapter are subsequent
phases. Until the adapter is registered, `--transport=falcon` remains intentionally unavailable to
workload experiments.
