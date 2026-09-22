# UEC wire-format migration

## Completed slice

The first migration slice implements and tests the exact layouts required by the current RUD, ROD,
and UUD behavioral subset:

- PDS Request, ACK, NACK, UUD, and common Control Packet headers;
- `PDS_NEG_ON_OFF` negotiation payload including `SYN_RETX_TRANSFER`;
- SES standard request header for multi-packet `UET_SEND`;
- SES medium request header for single-packet `UET_DATAGRAM_SEND`;
- PDS packet type, next-header, and SES opcode enumerations;
- fixed network-order byte vectors for every implemented layout;
- endpoint transmit and receive paths using those layouts end to end;
- simulation-only endpoint/path metadata moved to a non-wire ns-3 packet tag;
- ROD NACK fields encoded with `UET_ROD_OOO`, `nack_psn`, and `Expected_PSN` payload;
- ACK generation using `cack_psn` and signed `ack_psn_offset`;
- 32-byte ACK_CC generation and receive processing with MPR, 64-bit SACK, and NSCC ACK state;
- SYN establishment using independent initiator/target PDCIDs and overloaded `psn_offset`.

## Remaining integration work

1. Implement sender-side NSCC congestion-window and rate control using the ACK_CC feedback state.
2. Integrate Negotiation, probing, clear, and close CP behavior with the PDC state machine.
3. Implement the remaining mandatory AI Base SES request/response layouts and operations.
4. Add IPv4, IPv6, and UDP encapsulation choices, including entropy placement and UDP checksum rules.
5. Replace the internal `UetHeader` semantic record with a private decoded-request structure; this is a
   code cleanup and does not change the current wire image.

The current end-to-end paths no longer serialize the legacy envelope. Their packet size and field
placement come from UEC-defined PDS and SES headers.
