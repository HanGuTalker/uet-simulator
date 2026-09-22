# Reliable unordered delivery model

## Implemented minimum loop

`SendRudMessage` fragments a message at `PayloadMtu`, assigns monotonically increasing packet
sequence numbers, attaches a common UET header, and retains each packet until acknowledged.
The lower network model is connected with `SetTransmitCallback`; it may introduce propagation,
queueing, loss, duplication, or reordering without changing the RUD implementation.

The receiver:

- accepts fragments in any order;
- suppresses duplicate fragments while ACKing them again;
- emits a NACK when an arriving PSN reveals a sequence gap;
- ACKs every valid data packet after `AckDelay`;
- reassembles by message ID and fragment offset; and
- emits `MessageComplete` once all bytes through the END fragment are contiguous.

The sender cancels retained state through ACK_CC CACK/SACK, retransmits immediately on NACK, and uses the PDC
`RetransmissionTimeout` when no later packet exposes a gap. A retransmission sets the
`RETRANSMISSION` header flag. `MaxRetransmissions` defaults to eight; reaching it moves the PDC
to `ERROR` and cancels the PDC's remaining retransmission events.

`TimestampTag` carries the original submission time without changing the UET simulation header,
allowing `MessageComplete` to report end-to-end modeled latency.

## Current boundary

This milestone implements the minimum RUD data/recovery loop plus ACK_CC feedback and selective
retransmission-state release. It does not yet implement ACK coalescing, a send window, sender-side
NSCC congestion control, sequence wrap handling, receiver resource limits, or complete wire-level
control negotiation. Those behaviors remain explicit later milestones.

Run the loss-and-recovery example with:

```bash
python3 ./ns3 run uet-rud-example
```
