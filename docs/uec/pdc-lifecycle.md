# PDC lifecycle and packet dispatch

## Lifecycle

An endpoint owns a bounded table of PDC objects and allocates nonzero, endpoint-local PDC IDs.
The implemented transitions are:

```text
CLOSED  -> OPENING
OPENING -> ACTIVE | CLOSED | ERROR
ACTIVE  -> CLOSING | ERROR
CLOSING -> CLOSED | ERROR
ERROR   -> CLOSED
```

An invalid transition leaves the state unchanged and emits no `PdcStateChange` trace. A PDC may
be removed only while `CLOSED`. `MaxPdcCount` bounds the number of table entries.

The basic network establishment path is implemented for RUD and ROD Requests. The initiator and
target allocate independent nonzero PDCIDs. The initiating PDC remains `OPENING` while sending SYN
Requests, then becomes `ACTIVE` after an ACK/NACK supplies a nonzero target PDCID. The target maps
the source endpoint and source PDCID to one allocated PDC so repeated SYN packets do not allocate
duplicates. ROD establishment accepts only the derived `Start_PSN` as its first expected PSN.

The CP common header and `PDS_NEG_ON_OFF` payload have exact codecs, but lost-SYN recovery via a
Negotiation CP, the full NACK resource/error paths, Close Command/Request, and
`Syn_Retx_Safety_Time` are not yet wired into the lifecycle.

## Receive dispatch

`UetEndpoint::ReceivePacket` performs these checks in order:

1. The packet is large enough for a PDS prologue and carries the simulation routing tag.
2. The destination endpoint ID matches the receiving endpoint.
3. The packet-type-selected PDS header has a valid size, flags, and reserved fields.
4. A SYN Request maps to an existing inbound PDC or allocates one; a non-SYN packet names an
   existing destination PDCID.
5. The PDC is `ACTIVE` and its delivery mode matches the packet.
6. The selected SES header is valid for the implemented operation.

An accepted packet is recorded by exactly one PDC and emits `PacketRx`. A rejected packet returns
a specific `UetReceiveStatus`, changes no PDC receive state, and emits no successful receive trace.
