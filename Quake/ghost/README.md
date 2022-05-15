# Ghosts

The ghost feature shows the player from a demo file while you are playing the
game or watching another demo file.  This is useful for speedruns to know where
you are relative to a reference demo.

## Commands

- `ghost <demo-file>`:  Add ghost from the given demo file.  The ghost will be
  loaded on next map load.  With no arguments it will show the current ghost's
  demo file, if any.  Only one ghost may be added at a time.
- `ghost_remove`: Remove the current ghost, if any is added.  The change will
  take effect on next map load.

## Cvars

- `ghost_delta [0|1]`: Show how far ahead or behind the ghost you currently are.
- `ghost_range <distance>`:  Hide the ghost when it is within this distance.
- `ghost_alpha <float>`: Change how transparent the ghost is.  `0` is fully
  transparent, `1` is fully opaque.
