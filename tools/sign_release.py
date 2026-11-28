#!/usr/bin/env python3
"""Signs a RabbitMesh update file, so T-Decks will install it over WiFi.

    RABBITMESH_SIGNING_KEY=<64 hex> python3 tools/sign_release.py FILE        # -> FILE.sig
    python3 tools/sign_release.py --verify FILE                                # checks FILE.sig

The signature is Ed25519 over the file's SHA-256 (32 bytes), 64 bytes raw: what updcheck.cpp
checks against the public key in src/updcore.h before it switches to a downloaded update. The
key is the 32-byte Ed25519 seed, in hex, kept as the GitHub Actions secret RABBITMESH_SIGNING_KEY;
the release job (.github/workflows/release.yml) runs this. Needs: pip install cryptography.
"""
import hashlib
import os
import re
import sys

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey

HERE = os.path.dirname(os.path.abspath(__file__))


def public_key():
    src = open(os.path.join(HERE, "..", "src", "updcore.h")).read()
    block = re.search(r"RELEASE_KEY\[32\] = \{(.*?)\};", src, re.S).group(1)
    return bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", block))


def digest(path):
    return hashlib.sha256(open(path, "rb").read()).digest()


def main():
    args = sys.argv[1:]
    if len(args) == 2 and args[0] == "--verify":
        sig = open(args[1] + ".sig", "rb").read()
        try:
            Ed25519PublicKey.from_public_bytes(public_key()).verify(sig, digest(args[1]))
        except InvalidSignature:
            sys.exit("signature does NOT match the key in src/updcore.h")
        print("signature good")
        return
    if len(args) != 1:
        sys.exit(__doc__)
    seed = os.environ.get("RABBITMESH_SIGNING_KEY", "").strip()
    if not re.fullmatch(r"[0-9a-fA-F]{64}", seed):
        sys.exit("RABBITMESH_SIGNING_KEY isn't set (64 hex digits)")
    key = Ed25519PrivateKey.from_private_bytes(bytes.fromhex(seed))
    pub = key.public_key().public_bytes_raw()
    if pub != public_key():
        sys.exit("RABBITMESH_SIGNING_KEY isn't the key whose public half is in src/updcore.h")
    open(args[0] + ".sig", "wb").write(key.sign(digest(args[0])))
    print("signed", args[0])


if __name__ == "__main__":
    main()
