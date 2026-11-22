#!/usr/bin/env python3
"""seed_chroma_collection.py -- copy one program's semantic-search collection to a new program name in a
project's chromadb, so pyghidra-mcp finds the collection at startup and skips its full code ingest.

Why: pyghidra-mcp decompiles EVERY function of a program that has no collection (68k functions for the
clean-TU5 program) on the live service's JVM, in the background, competing with every other lane's
requests. The clean-TU5 and Deluxe programs share every address, so the Deluxe collection is a usable
stand-in (its decompiles carry the names that were applied when it was built). A real re-ingest is a
deliberate, separate step.

Run in the pyghidra-mcp venv, on a COPY of the project (never the live one while the service runs):
  uv run --python 3.10 --project ../pyghidra-mcp python tools/ghidra/seed_chroma_collection.py \
      --chroma ~/tmp/ghidra-sync/proj3/RB3Xenon/chromadb \
      --from default_tu5.xex-c5a170 --to default.xex-d56e7f
"""
import argparse
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--chroma", required=True)
    ap.add_argument("--from", dest="src", required=True)
    ap.add_argument("--to", dest="dst", required=True)
    args = ap.parse_args()

    import chromadb
    client = chromadb.PersistentClient(path=args.chroma)
    names = [getattr(c, "name", c) for c in client.list_collections()]
    print("[seed_chroma] collections before: %s" % names)
    if args.dst in names:
        print("[seed_chroma] REFUSING: %s already exists" % args.dst)
        return 2
    src = client.get_collection(args.src)
    n = src.count()
    dst = client.create_collection(name=args.dst)
    step = 5000
    for off in range(0, n, step):
        got = src.get(offset=off, limit=step, include=["embeddings", "documents", "metadatas"])
        dst.add(ids=got["ids"], embeddings=got["embeddings"], documents=got["documents"],
                metadatas=got["metadatas"])
    print("[seed_chroma] %s (%d) -> %s (%d)" % (args.src, n, args.dst, dst.count()))
    return 0 if dst.count() == n else 1


if __name__ == "__main__":
    sys.exit(main())
