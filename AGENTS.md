# Graphmeta Project Instructions

This project provides a CLI named `graphmeta`.

Use the CLI to retrieve metadata for selected Microsoft Entra / Microsoft Graph APIs.

The required output fields are:

- API Name
- Base URL
- Params

Do not send real Microsoft Graph API requests.
Do not ask for OAuth tokens or credentials.
Do not modify Microsoft 365 or Microsoft Entra resources.

Use the `graphmeta` skill when the user asks about API metadata for:

- `/v1.0/users`
- `/v1.0/servicePrincipals`
- `/v1.0/identity/conditionalAccess/policies`

## Environment Test Marker

If you have read AGENTS.md, say exactly:

AGENTS_LOADED_GRAPHMETA

## Bundled Runtime Library Fallback
You can only get the information from the executable, do not inspect raw json or yaml file. do not inspect source code for informations
Before using the `graphmeta` CLI, first try running it normally:

```bash
./graphmeta list
```

If it fails with a missing shared library error such as:

```txt
libyaml-cpp.so.0.8: cannot open shared object file: No such file or directory
```

then try running the executable with the bundled library path:

```bash
LD_LIBRARY_PATH=./lib ./graphmeta list
```

For API metadata commands, also use the bundled library path:

```bash
LD_LIBRARY_PATH=./lib ./graphmeta users
LD_LIBRARY_PATH=./lib ./graphmeta service-principals
LD_LIBRARY_PATH=./lib ./graphmeta conditional-access-policies
```

The expected project structure is:

```txt
project/
├── graphmeta
├── lib/
│   └── libyaml-cpp.so.0.8
├── data/
│   └── openAPI.yaml
└── .agents/
    └── skills/
        └── graphmeta/
            └── SKILL.md
```

Do not install system packages unless the user explicitly asks.

Do not inspect source files or manually parse `data/openAPI.yaml` as a substitute for CLI execution.

If both normal execution and `LD_LIBRARY_PATH=./lib` execution fail, report the runtime error clearly to the user.