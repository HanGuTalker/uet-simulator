# Reliable ordered delivery model

## Normative basis

The model follows UEC Specification 1.0.3 sections 3.5.7.2 and 3.5.21.1. ROD uses one network
path and Go-Back-N recovery. The destination tracks `Expected_PSN` and:

- accepts a request only when its PSN equals `Expected_PSN`;
- drops a request whose PSN is greater than `Expected_PSN` and returns a NACK identifying the
  expected PSN;
- ACKs an already delivered duplicate without delivering it again; and
- advances `Expected_PSN` only after accepting the expected request.

When the source receives the first out-of-order NACK for a loss event, it retransmits every
outstanding packet starting at the expected PSN. Further NACKs for the same event do not start a
second Go-Back-N pass before an ACK arrives. Retransmissions retain their PSNs and set the
`RETRANSMISSION` flag.

## Verified scenario

The unit test transmits three single-packet messages and drops PSN 1. PSN 2 and PSN 3 are rejected
as out of order. Their NACKs report PSN 1 as the expected sequence, and the source retransmits PSNs
1, 2, and 3 in order. A duplicate retransmitted PSN 2 is also injected after delivery and must not
produce a second completion. The test verifies completion order 1, 2, 3 and an empty sender
outstanding table.

## Current boundary

ROD Request, ACK, and NACK packets now use their UEC-defined PDS layouts. An out-of-order NACK
carries `UET_ROD_OOO`, the triggering PSN in `nack_psn`, and `Expected_PSN` in its payload. ACKs
carry `cack_psn` and signed `ack_psn_offset`.

ACK_CC and the 64-bit SACK field are integrated; ROD may legally emit a zero bitmap because CACK and
ACK_PSN contain the required ordering information. ACK coalescing, ACK Request CP recovery,
guaranteed-delivery SES responses, entropy/IP/UDP encapsulation, and
sender-side congestion-control interaction remain later work.
