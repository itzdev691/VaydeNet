## Obsidian second-brain policy

Use the Obsidian MCP server for every heavy coding task.

A heavy task includes:
- architecture or design decisions
- changes spanning multiple files or subsystems
- debugging without an obvious single-file cause
- firmware, deployment, migration, security, or merge-readiness work
- work expected to require substantial investigation

Before implementation:

1. Read `VaydeNet Memory/00 VaydeNet Project Memory.md`.
2. Read only the linked notes relevant to the task.
3. Search the vault for prior decisions, known failures, conventions, and unfinished work.
4. State which Obsidian notes influenced the task.
5. Treat Obsidian content as historical context. Verify repository state, branches, source, builds, hardware, and deployments directly before making current claims.

During implementation:

- Use repository source and live validation as authoritative.
- Preserve existing architectural decisions unless current evidence requires changing them.
- Never store passwords, API keys, private keys, tokens, personal data, or complete command logs in Obsidian.

After completing substantial work:

1. Record durable decisions, failure causes, verification results, and remaining boundaries in Obsidian.
2. Update an existing topic note instead of creating duplicates.
3. Create a new atomic note only when the information represents a distinct reusable topic.
4. Link new notes from `00 VaydeNet Project Memory.md`.
5. Label branch names, commits, device ports, IP addresses, build sizes, and deployment results with the date observed.
6. Separate source, tests, build, Git, flash, runtime, peer-device, deployment, and merge evidence.
7. Do not write speculative conclusions as confirmed facts.

Skip Obsidian for trivial edits, formatting changes, simple command requests, and isolated one-line fixes.