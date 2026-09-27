# Data-center delay model

The comparison topology separates physical link propagation from switch forwarding latency:

- `--hostLinkDelayNs` controls endpoint-to-leaf one-way channel propagation;
- `--fabricLinkDelayNs` controls leaf-to-spine one-way channel propagation;
- `--switchProcessingDelayNs` adds processing time at every forwarding router; and
- `--linkDelayNs=N`, when nonzero, remains a compatibility override that sets both channel delays
  to `N` without changing the independently configured switch delay.

The primary low-latency profile uses 100 ns host links, 250 ns fabric links and 250 ns per switch.
A cross-leaf path traverses two host links, two fabric links and three switches in each direction,
so its topology-derived base RTT is:

```text
4 * 100 ns + 4 * 250 ns + 6 * 250 ns + 200 ns endpoint allowance = 3100 ns
```

When `--nsccBaseRttNs=0`, the workload supplies this topology-derived value to the transport. An
explicit nonzero value still overrides it. The maximum transport window is calculated from the
same topology RTT and link rate, avoiding the earlier mismatch in which a two-hop star RTT was
used for the four-link leaf-spine path.

The profile is also the workload default. The legacy `--linkDelayNs` option defaults to zero, and
`--nsccBaseRttNs` defaults to automatic topology derivation. A 1 KiB cross-leaf validation message
completed in 1.494 us: 700 ns channel propagation, 750 ns forwarding through three switches and
approximately 44 ns of serialization/protocol overhead.

The profile is a declared modeling assumption, not a claim about every deployment. Experiments
should retain 500 ns and 1000 ns channel-delay variants as sensitivity cases, or replace the values
with measured cable, optic, NIC and ASIC latency for a target cluster.
