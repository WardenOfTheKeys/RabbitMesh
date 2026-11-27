#!/usr/bin/env python3
"""Keeps RabbitMesh on the newest MeshCore release.

    python3 tools/bump_meshcore.py            # say which MeshCore it's on and which is newest
    python3 tools/bump_meshcore.py --apply    # move the pin to the newest release

RabbitMesh builds on an exact MeshCore commit, pinned in patches/rabbitmesh-tdeck.patch (the
`+ ...MeshCore.git#<commit>` line it adds to platformio.ini). This looks up MeshCore's
companion releases (git tags companion-vX.Y.Z), and with --apply puts the newest one's commit
in that line and its version in docs/DEVELOPMENT.md. Then build and test: MeshCore's own
changes can need fixes in tools/patch_meshcore.py or src/node.cpp. The weekly GitHub job
(.github/workflows/meshcore-update.yml) runs this, builds, tests and opens a pull request.

--github-output FILE also writes changed=, version=, sha= and current= for a GitHub job.
Needs git and Python 3; nothing to install.
"""
import argparse
import os
import re
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
PATCH = os.path.join(ROOT, "patches", "rabbitmesh-tdeck.patch")
DOC = os.path.join(ROOT, "docs", "DEVELOPMENT.md")
REPO = "https://github.com/meshcore-dev/MeshCore.git"
PIN = re.compile(r"^(\+\s+https://github\.com/meshcore-dev/MeshCore\.git#)([0-9a-f]{40})\s*$", re.M)
DOC_VERSION = re.compile(r"MeshCore companion \*\*v[0-9.]+\*\* \(`[0-9a-f]{7}`\)")


def releases():
    """{version tuple: (tag, commit)} for every companion-vX.Y.Z tag."""
    out = subprocess.run(["git", "ls-remote", "--tags", REPO, "refs/tags/companion-v*"],
                         check=True, capture_output=True, text=True).stdout
    tags = {}
    for line in out.splitlines():
        sha, ref = line.split("\t")
        tag = ref[len("refs/tags/"):]
        peeled = tag.endswith("^{}")
        tag = tag[:-3] if peeled else tag
        m = re.fullmatch(r"companion-v(\d+)\.(\d+)\.(\d+)", tag)
        if not m:
            continue
        key = tuple(int(x) for x in m.groups())
        if peeled or key not in tags:              # an annotated tag: its commit is the peeled line
            tags[key] = (tag, sha)
    return tags


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apply", action="store_true", help="move the pin to the newest release")
    ap.add_argument("--github-output", help="also write key=value lines here (GitHub Actions)")
    args = ap.parse_args()

    patch = open(PATCH).read()
    m = PIN.search(patch)
    if not m:
        raise SystemExit("couldn't find the MeshCore pin in " + PATCH)
    current = m.group(2)
    tags = releases()
    if not tags:
        raise SystemExit("no companion-v* releases found at " + REPO)
    newest = max(tags)
    tag, sha = tags[newest]
    version = "v%d.%d.%d" % newest
    cur_name = next((t for t, s in tags.values() if s == current), current[:7])
    changed = sha != current
    print(f"RabbitMesh is on MeshCore {cur_name}; the newest release is {tag} ({sha[:7]})"
          + ("" if changed else " - up to date"))

    if changed and args.apply:
        open(PATCH, "w").write(PIN.sub(lambda x: x.group(1) + sha, patch, count=1))
        doc = open(DOC).read()
        new_doc = DOC_VERSION.sub(f"MeshCore companion **{version}** (`{sha[:7]}`)", doc, count=1)
        open(DOC, "w").write(new_doc)
        print(f"pin moved to {tag}; now build (setup.sh, pio run) and run test/run.sh")

    if args.github_output:
        with open(args.github_output, "a") as f:
            f.write(f"changed={'true' if changed else 'false'}\nversion={version}\nsha={sha}\ncurrent={cur_name}\n")


if __name__ == "__main__":
    main()
