# UEC 1.0.3 AI Base conformance audit

## Result

All 20 mandatory rows in the AI Base column of the UEC Transport Matrix and Checklist revision 0.8
have executable implementations and tests. The detailed mapping is in `ai-base-coverage.md` and the
machine-readable status is in `requirements/ai-base.yaml`.

“Verified” means verified inside this ns-3 behavioral model. It does not mean UEC certification,
production-NIC interoperability, or validation against a proprietary hardware implementation.

## Protocol coverage

| Area | Implemented model |
|---|---|
| SES | NO_OP, SEND, DATAGRAM SEND, WRITE, WRITE immediate, non-fetching atomic, relative/absolute addressing, authorization and memory keys |
| SES wire formats | standard request, medium request, atomic extension, response and Delivery Complete/GO |
| RUD | once-only receive, CACK/SACK, duplicate suppression, NACK/RTO recovery, retry limit, retained response and CLEAR_PSN |
| ROD | single-path ordered delivery, out-of-order drop, UET_ROD_OOO NACK and Go-Back-N recovery |
| UUD | one connectionless datagram, no PDC, acknowledgement or retransmission state |
| PDC | independent local IDs, SYN establishment/retry, active/closing/closed lifecycle and clear/close/control handling |
| NSCC | destination ACK state, source cwnd/rate response, ECN/loss/delay inputs, pacing and transmit gating |
| Trimming | endpoint detection, UET_TRIMMED NACK/recovery; trimmed SYN and control/feedback packets are discarded |
| Network | UDP over IPv4 and IPv6, path MTU, ECT/CE propagation and CRC32C payload-integrity model |

PSN comparisons use 32-bit serial arithmetic and are tested across wrap for both RUD and ROD.
RUD recovery is independently tested for NACK, timeout, ACK loss, request loss, reordering and two
simultaneous PDCs. ROD does not buffer and selectively recover out-of-order packets: it drops them
and retransmits from Expected_PSN as required by Go-Back-N.

## 1.0.3-specific behavior

The model retains the 1.0.3 boolean negotiation payload and does not generate the removed
trimmed-ACK NACK behavior. A trimmed packet cannot establish a new PDC. SYN is retained and retried
under the normal reliable-request timeout/retry safety mechanism.

## Deliberate fidelity boundaries

The module targets protocol and performance simulation. NIC DMA, PCIe, memory-controller, switch
pipeline, PHY/FEC and SerDes details are parameterized or delegated to ns-3 devices. CRC32C is
computed over the UET UDP payload because the endpoint module does not own the final IP mutable
header. Optional profile capabilities—RUDI, RCCC, TSS, READ, tagged operations, fetching atomics,
rendezvous and in-network collectives—are excluded and are not prerequisites for the AI Base
mandatory column.
