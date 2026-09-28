# Three-tier fat-tree topology

## Construction

The common workload runner implements an even-k, three-tier Clos/fat-tree with
`--fabric=fat-tree --fatTreeK=k`. The endpoint count must be exactly `k^3/4`; the current command-line
guard accepts k=4, 6, or 8. A k-ary instance contains k pods, `k^2/2` edge switches, `k^2/2`
aggregation switches, `k^2/4` core switches and k/2 endpoints below every edge switch.

The initial comparison topology uses k=4:

```text
                         4 core switches
                    C0   C1   C2   C3
                     |\ /|    |\ /|
       +-------------+ X +----+ X +-------------+
       |             |/ \|    |/ \|             |
   pod 0            pod 1     pod 2            pod 3
  A0   A1          A2   A3   A4   A5          A6   A7
   | X |            | X |     | X |            | X |
  E0   E1          E2   E3   E4   E5          E6   E7
  /\   /\          /\   /\   /\   /\          /\   /\
 H0 H1 H2 H3      H4 H5 H6 H7 H8 H9 H10 H11 H12 H13 H14 H15
```

Within a pod, every edge connects to both aggregation switches. Aggregation switch position 0 in
each pod connects to core group C0/C1, and position 1 connects to C2/C3. Every host, edge-to-
aggregation and aggregation-to-core link is full duplex, uses the configured data rate, and has a
9000-byte MTU. Each point-to-point link receives a distinct /30 IPv4 subnet.

## Delay and forwarding model

The 800 Gbps validation uses 100 ns host links, 250 ns fabric links and 250 ns forwarding delay at
every IP switch. The topology-derived worst-case cross-pod RTT is therefore:

```text
4 * host-link + 8 * fabric-link + 10 * switch-processing + 200 ns endpoint allowance
= 5100 ns
```

Global IPv4 five-tuple ECMP is enabled. The forwarding node identifier is included as a stable
per-stage hash salt: a flow remains pinned at each switch, but its edge and aggregation choices are
not forced to use the same hash low bits. Protocols that vary UDP source-port entropy can still
exercise packet spraying or rerouting.

`-fabric-paths.csv` traces edge-to-aggregation and aggregation-to-core transmissions. Only the first
fabric hop has `wire_accounting=1`, preventing the same packet from being counted twice when
calculating aggregate modeled wire rate. Both tiers remain available for path-diversity analysis.

## Reproduction and readiness result

Build the optimized example and run:

```bash
./ns3 build uet-ai-workload-example
bash scripts/transport/run_fat_tree_validation_800g.sh
```

The deterministic readiness run sends one 1 KiB cross-pod UEC message and then one directed 1 KiB
message for every ordered endpoint pair. The verified k=4 result is:

| Workload | Completion | Goodput | Mean latency | Fabric coverage |
|---|---:|---:|---:|---|
| Cross-pod single | 1/1 | 3.256 Gbps | 2.516 us | 2 aggregations, 1 core |
| 16-node All-to-All | 240/240 | 111.475 Gbps | 2.320 us | 8 aggregations, 4 cores |

This is a topology and routing readiness check, not a protocol performance comparison. Subsequent
experiments should keep topology, traffic matrix, delays, queue configuration and seeds identical
across UEC, RoCEv2, veRoCE, MRC and Falcon. The first such experiment is documented in
`docs/transport/fat-tree-uncongested-baseline-800g.md`.
