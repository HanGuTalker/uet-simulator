# Packet model

## Normative wire codecs

The module now contains the first UEC 1.0.3 bit-accurate wire codecs. Multi-byte values use network
byte order and the field order follows the figures and tables in the cited specification sections.

| Class | Size | UEC format | Specification |
|---|---:|---|---|
| `UetPdsHeader` | 12 bytes | RUD/ROD Request | 3.5.10.3, Figure 3-52 |
| `UetPdsHeader` | 12 bytes | RUD/ROD ACK | 3.5.10.5, Figure 3-54 |
| `UetPdsHeader` | 32 bytes | RUD/ROD ACK_CC with 64-bit SACK | 3.5.10.6, Figure 3-55 |
| `UetPdsHeader` | 16 bytes | NACK | 3.5.10.10, Figure 3-59 |
| `UetPdsHeader` | 4 bytes | UUD Request | 3.5.10.12, Figure 3-61 |
| `UetPdsHeader` | 12 bytes | RUD/ROD Control Packet | 3.5.10.8, Figure 3-57 |
| `UetNegotiationOnOffHeader` | 8 bytes | `PDS_NEG_ON_OFF` payload | 3.5.16.7.1, Figure 3-78 |
| `UetSesStandardHeader` | 44 bytes | Standard Request | 3.4.2.1, Figures 3-9 and 3-10 |
| `UetSesMediumHeader` | 32 bytes | Medium Request | 3.4.2.2, Figure 3-14 |
| `UetAtomicExtensionHeader` | 4 bytes | Atomic Extension | 3.4.2.3, Figure 3-16 |
| `UetSesResponseHeader` | 16 bytes | Standard Response | 3.4.2.4, Figure 3-18 |

The PDS prologue is serialized as a 16-bit network-order value containing `type[5]`,
`next_hdr/ctl_type[4]`, and `flags[7]`. Reserved transmit fields are written as zero. Fixed byte-vector
tests check field placement independently of round-trip deserialization.

## Endpoint integration

The RUD, ROD, ACK, NACK, and UUD endpoint paths now serialize only the applicable PDS and SES
headers above. `UetHeader` remains as an internal, non-serialized semantic record used between the
decoder and reliability state machine; it no longer contributes bytes to transmitted packets.

An initiating PDC in `OPENING` sends Request packets with `syn=1`, a nonzero source PDCID, and the
overloaded `{pdc_info, psn_offset}` destination field. The target keys duplicate establishment
requests by source endpoint and source PDCID, allocates an independent target PDCID, derives
`Start_PSN`, and returns its PDCID in ACK/NACK. The initiator learns that ID and uses it as `dpdcid`
with `syn=0` on subsequent requests and retransmissions.

Reliable receive paths generate `ACK_CC` with `cc_type=CC_NSCC`. The extension carries MPR, a
signed `sack_psn_offset`, the 64-bit SACK bitmap, and the 64-bit NSCC destination state described in
section 3.6.9.2. The sender processes the triggering ACK PSN first, releases all retransmission state
through CACK, and then releases the PSNs selected by the bitmap. Service time is encoded in 128 ns
units; a zero value is used when the modeled ACK delay is zero and therefore means "not valid" as in
the specification. The current `rcvd_bytes` input counts accepted semantic payload bytes in 256-byte
units until IP encapsulation supplies a complete network-byte accounting model.

PSN ordering uses 32-bit serial-number arithmetic. Forward order, cumulative ACK release, SACK
selection, duplicate detection, and ROD Go-Back-N therefore remain valid across
`0xffffffff -> 0`. As required by serial-number arithmetic, one active comparison window must stay
below half of the 32-bit sequence space. `UetPdc::ConfigureStartPsn` provides a deterministic
experiment input; otherwise an opening PDC retains the modeled randomized `Start_PSN` behavior.

Every reliable Request carries `CLEAR_PSN` as a signed 16-bit offset from its PSN. The source
advances this value only through contiguously received ACK PSNs. The target uses it to release older
retained response-replay state, including across PSN wrap. The current model conservatively retains
the ACK response state for each accepted SEND; explicit Clear Request/Command CP recovery remains
separate work.

`UetUdpTransport` carries these bytes through ns-3 UDP over either IPv4 or IPv6. It enforces a
configurable path MTU, sets ECT(0), imports CE marking into ACK_CC processing, and appends/verifies a
CRC32C trailer. Endpoint IDs, path IDs, trim metadata, and timestamps remain simulation metadata in
`UetSimulationTag`; they do not contribute to wire size.

The mandatory AI Base formats are byte-tested. ACK_CCX belongs to optional extended behavior and is
not emitted by this profile. The CRC abstraction covers the UET payload visible above UDP; the ns-3
IP stack owns IP mutable fields and UDP checksum processing.
