---
created: 2026-06-30T08:27:51.829Z
title: Restore encrypted BLE bond on StickS3
area: firmware
files: []
---

## Problem

After removing + re-pairing the buddy in Windows Bluetooth (done to clear a stale GATT cache
that was blocking the notify subscribe), the bridge reconnected but logs:
`stick link: UNENCRYPTED — transcript sniffable!` (previously it was `ENCRYPTED`). So the
re-pair came back as an unencrypted/unbonded link. Functionally it works, but the BLE transcript
(session data) is now sniffable — a security regression vs the prior bonded state.

## Solution

Re-establish an encrypted bond between the PC and the StickS3:
- Try `cc-buddy-bridge unpair` (clears the stick's stored bond), then Forget the device in
  Windows Bluetooth, then re-pair so a fresh encrypted bond is negotiated.
- Confirm the bridge daemon logs `stick link: ENCRYPTED` after reconnect.
- If it keeps coming back unencrypted, check the StickS3 firmware BLE security/bonding config
  (the GATT notify characteristic should require encryption) — the Phase 3 port may not be
  requesting/enforcing bonding the way the original did.
