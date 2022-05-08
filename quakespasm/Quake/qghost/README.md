# Quake demo to ghost converter

Convert quake demo (.dem) files into ghost files (.ghost) for use with this
Quakespasm branch.

Example usage:

```bash
python qghost.py ~/.quakespasm/id1/e1m1_022.dem > ~/.quakespasm/id1/ghosts/e1m1.ghost
```

The modified Quakespasm will pick up ghost files in `<game-dir>/ghosts/` and
will load the ghost file using the map name whenever you load a map or play a
demo.

This is only lightly tested and could probably do with a nicer interface, but
feel free to do as you please with it, subject to the terms of the license.

## Dependencies

PyQuake: https://github.com/matthewearl/pyquake/

## Quakespasm code

See [cl_ghost.c](../cl_ghost.c) for the bulk of the changes.  Look for callers
into these functions for the rest of the relevant changes.  There's a load of
other junk on this branch so don't attempt a wholesale merge.
