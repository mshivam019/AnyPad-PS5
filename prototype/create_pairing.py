#!/usr/bin/env python3
"""Generate a matched pairing token for the sender and console ELF before building."""
import json
from pathlib import Path
import secrets

folder = Path(__file__).resolve().parent
if (folder / "pairing.json").exists():
    token = bytes.fromhex(json.loads((folder / "pairing.json").read_text())["token"])
else:
    token = secrets.token_bytes(16)
    (folder / "pairing.json").write_text(json.dumps({"token": token.hex()}, indent=2)+"\n")
if len(token) != 16:
    raise ValueError("Pairing token must be 16 bytes")
(folder / "pairing.h").write_text(
    "/* Generated local pairing token. GPL-3.0-or-later. */\n"
    "static const unsigned char rg_token[16] = {" + ",".join(str(v) for v in token) + "};\n")
print("Pairing files ready. Rebuild the ELF if you change pairing.json.")
