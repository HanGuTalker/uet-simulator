# Multi-spine ECMP comparison fabric

## Topology and forwarding

The common workload runner provides a physical two-leaf, configurable multi-spine fabric. Half of
the endpoints attach to each leaf and every leaf has one point-to-point link to every spine.
`--fabric=leaf-spine --spines=N` creates `N` equal-cost paths; all endpoint and fabric links use the
rate selected by `--linkRate`.

IPv4 global routing uses a stable hash of source address, destination address, IP protocol, UDP
source port and UDP destination port. Packets in the same five-tuple therefore stay on one spine,
while a different UDP source-port entropy value can select another spine. This supplies the
forwarding behavior required to compare these transport strategies:

- UEC and RoCEv2 use flow-pinned forwarding in the current models;
- veRoCE and MRC vary UDP source-port entropy for packet spraying;
- Falcon remains flow-pinned until PLB changes its UDP entropy path.

The runner records every packet transmitted on a leaf-to-spine link in `-fabric-paths.csv` as
`time_ns,leaf_id,spine_id,wire_bytes`. The `active_spines` and `fabric_path_packets` summary fields
provide an aggregate check. These counters include forward data and reverse control traffic.

## Reproducible 800 Gbps entry point

Run the short readiness matrix with:

```bash
bash scripts/transport/run_ecmp_comparison_800g.sh
```

It runs UEC, RoCEv2, veRoCE, MRC and Falcon with identical four-node, four-spine, 800 Gbps,
all-to-all inputs and writes a combined CSV to
`results/ecmp-comparison-800g/comparison-summary.csv`. Environment variables can scale the same
entry point without editing it, for example:

```bash
NODES=8 SPINES=4 MESSAGES=4 PAYLOAD_BYTES=4194304 \
PATTERNS="incast all-to-all ring-allreduce" \
bash scripts/transport/run_ecmp_comparison_800g.sh results/ecmp-main-800g
```

MetaRoCE is deliberately not included: its public normative detail is still insufficient for a
defensible independent wire/transport implementation, and the registry continues to mark it as
planned rather than silently substituting another model.

## Readiness result

The four-node/four-spine 64 KiB all-to-all check completed all 12 messages for every available
protocol. The result is a functional topology/readiness check, not a performance ranking:

| Protocol | Completion | Goodput (Gbps) | Mean latency (us) | P99 (us) | Active spines |
|---|---:|---:|---:|---:|---:|
| UEC | 12/12 | 140.944 | 12.753 | 14.650 | 4 |
| RoCEv2 | 12/12 | 148.492 | 10.314 | 12.410 | 3 |
| veRoCE | 12/12 | 148.006 | 10.451 | 12.676 | 4 |
| MRC | 12/12 | 148.006 | 10.451 | 12.676 | 4 |
| Falcon | 12/12 | 148.446 | 10.328 | 12.462 | 4 |

An additional one-connection, cross-leaf 1 MiB check validates forwarding semantics. UEC,
RoCEv2 and uncongested Falcon each selected one physical spine per direction (two in the aggregate,
because data and acknowledgements hash independently). veRoCE and MRC used four physical spines
and reported four active transport paths, demonstrating that their source-port packet spraying is
visible in the network rather than only in endpoint metadata.

Formal comparisons should add multiple seeds, offered-load/message-size sweeps and congested
incast/collective workloads. Report protocol implementation scope alongside results: veRoCE, MRC
and Falcon are documented comparison subsets, not full-product conformance models.
