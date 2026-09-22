# Trace contract

The trace names and payload order below form the initial stable experiment interface. Trace
payloads use integer identifiers rather than pointers to protocol state, so result files remain
portable across runs.

| Trace | Payload |
| --- | --- |
| `PacketTx` | packet, PDC ID, path ID |
| `PacketRx` | packet, PDC ID, path ID |
| `PdcStateChange` | PDC ID, old state, new state |
| `Ack` | PDC ID, PSN |
| `Nack` | PDC ID, PSN |
| `Timeout` | PDC ID, PSN |
| `Retransmission` | PDC ID, PSN |
| `CongestionWindow` | PDC ID, old bytes, new bytes |
| `EcnReceived` | PDC ID, path ID |
| `PathSelected` | PDC ID, PSN, path ID |
| `PacketTrimmed` | packet, PDC ID, path ID |
| `ReorderDepth` | PDC ID, old depth, new depth |
| `MessageComplete` | PDC ID, message ID, bytes, latency |

Endpoint trace configuration path:

```text
/NodeList/*/$ns3::UetEndpoint/<TraceName>
```

Trace names or payload order may only change with an entry in the project change log because
experiment parsers depend on them.
