---
name: ghidra-search
description: Semantic search over Ghidra decompiled code. Use when looking for similar implementations, finding functions by behavior description, or searching for code patterns. Automatically filters out __unwind$ and other noise.
argument-hint: "[natural language query]"
allowed-tools: Bash(python3 tools/ghidra/code_search.py *)
---

# Ghidra Code Search

Search the ChromaDB vector index of decompiled code with semantic understanding.

## Arguments

`$ARGUMENTS`

## Usage

```bash
python3 tools/ghidra/code_search.py "QUERY" [--limit N] [--exclude PATTERN]
```

The Ghidra MCP server for rb3-xenon runs on port **8002**.

## Steps

1. **Formulate the search query** based on `$ARGUMENTS`:
   - Use natural language descriptions of behavior
   - Or code snippets you want to find similar implementations of

2. **Run the search:**
   ```bash
   python3 tools/ghidra/code_search.py "your query here" --limit 10
   ```

3. **Filter noise if needed:**
   ```bash
   python3 tools/ghidra/code_search.py "query" --exclude "thunk" --exclude "vtable"
   ```

4. **Present results** showing:
   - Function name and address
   - Similarity score
   - Decompiled code snippet

## Examples

| Query | Finds |
|-------|-------|
| "poll task list and run callbacks" | Poll functions that iterate tasks |
| "handle message switch symbol" | Handle() methods with message routing |
| "load file parse config" | File loading and parsing code |
| "for (i = 0; i < count; i++) { arr[i]->Save(bs); }" | Similar save loops |

## Tips

- Default filtering excludes `__unwind$`, `__ehhandler$`, `_GLOBAL_`, `$vectored` noise
- Use `--no-default-exclude` to see all results including noise
- Use `--strings` flag to search string literals instead of code
- Semantic search works best with behavioral descriptions, not exact names
