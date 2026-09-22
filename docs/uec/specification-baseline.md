# Specification baseline

## Pinned baseline

- Ultra Ethernet Specification: **1.0.3**, released 2026-07-16.
- Implemented profile: **AI Base** only.
- Simulator baseline: **ns-3.47**, tag `ns-3.47`.
- Development branch: `uec-ai-base`.

The UEC specification is the normative source. The public Transport Matrix and Checklist,
revision 0.8 dated 2025-06-10, is used as a requirements index only because it predates
Specification 1.0.3. Every implementation requirement must therefore retain a specification
section reference and be checked against the 1.0.1, 1.0.2, and 1.0.3 release notes.

## AI Base feature baseline

The initial feature-level mandatory set is:

- SES: NO_OP, SEND, DATAGRAM SEND, WRITE, WRITE IMM, non-fetching ATOMIC.
- SES addressing and protection: relative addressing, absolute addressing, authorization
  semantics, and memory keys.
- SES formats: Standard, Deferrable Send as Send, Atomic Extension, Response, and Delivery
  Complete (GO).
- PDS: RUD, ROD, UUD, and endpoint support for trimmed packets.
- CMS: NSCC for best-effort networks.

Optional features are not pulled into scope by dependency without an explicit design decision.

## 1.0.3 deltas that must remain visible

- `Syn_Retx_Safety_Time` behavior for closing and reopening a PDC.
- Negotiation control packet boolean (`on_off`) type.
- `Override_MPR_Zero_Value` and `MP_Range` values below 128.
- Trimmed ACK handling; a trimmed ACK does not generate a `UET_TRIMMED_ACK` NACK.

## Sources

- Specification and compliance material: <https://ultraethernet.org/compliance/>
- Specification 1.0.3 release notes:
  <https://ultraethernet.org/wp-content/uploads/sites/20/2026/07/UE-Specification-1.0.3-release-notes-.pdf>
- Public Transport Matrix and Checklist:
  <https://ultraethernet.org/wp-content/uploads/sites/20/2025/06/Transport_Matrix_and_Checklist.xlsx>
