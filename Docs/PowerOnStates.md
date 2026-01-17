# PowerOnStates

## Apple IIe



## Apple IIgs

| Addr | Register | Our Value | KEGS Value | Notes |
|-|-|-|-|
| C068 | State | 0D | 0D | KEGS sets this to 0C for ROM00, 0D for ROM01/03, but I found reference to 0C |
| C036 | Speed | 00 | ?? | I have a comment that this is set to 00 on powerup/reset |
| C029 | New Video | 00 | ?? | reading the bits, it would make sense for these to all be 0 to force back into emulated state  (bit 0 =1 maybe?) |
| C022 | Text Color | F0 | ?? | holding ctrl-reset the text goes to white on black background |
