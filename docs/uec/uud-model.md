# UUD behavioral model

## Normative basis

UEC 1.0.3 sections 3.4.1.13, 3.5.6, 3.5.7.4, and 3.5.10.12 define unreliable
unordered delivery (UUD). The implemented subset preserves these protocol semantics:

- a UUD operation carries one `UET_DATAGRAM_SEND` packet;
- UUD is connectionless and does not allocate or use a PDC;
- the datagram has no PSN or message ID state;
- loss is not recovered and the receiver emits no ACK or NACK;
- the payload must not exceed `PayloadMtu`.

`SendUudDatagram` therefore emits one 4-byte PDS UUD Request header followed by one 32-byte
`UET_HDR_REQUEST_MEDIUM` SES header with opcode `UET_DATAGRAM_SEND`. It carries no PDC ID, PSN,
or message ID. A valid received datagram produces one `PacketRx` and one `MessageComplete` trace; a
lost datagram produces neither completion nor retransmission.

## Fidelity boundary

IP/UDP encapsulation and real endpoint addressing are not yet modeled. Simulation endpoint IDs and
the selected path are carried in an ns-3 packet tag and therefore do not affect the modeled wire size.
