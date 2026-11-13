# Nofrendo source

Vendored from https://github.com/ducalex/retro-go, commit
`4ced120669750ca7228fd0414211430c1d923166`, directory
`retro-core/components/nofrendo`. Original notices, COPYING and CREDITS retained.

Local port: ESP-IDF component registration; no RETRO_GO dependency. CRC32 is
implemented locally. Truncated iNES images are rejected before bank pointers
are exposed. Runtime uses the memory loader, never opens or writes ROM files.

Additional correctness fixes: restore iNES trainers at CPU $7000 on hard reset;
return initialization failure when the APU PCM allocation fails; palette allocation
checks and C/C++ compatible ESP IRAM attributes.
