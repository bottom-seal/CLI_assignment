---
name: graphmeta
description: Use the graphmeta CLI to retrieve Microsoft Graph / Microsoft Entra API metadata for users, service principals, and conditional access policies, including API Name, Base URL, and Params.
---

## Environment Test Marker

If this graphmeta skill is loaded, say exactly:

SKILL_LOADED_GRAPHMETA

# Graphmeta Skill

## Overview

Use the local `graphmeta` CLI to retrieve metadata for selected Microsoft Graph / Microsoft Entra APIs.

The CLI is for metadata lookup only. It does not send real Microsoft Graph API requests and does not require authentication.

The required output fields are:

- API Name
- Base URL
- Params

## When to Use

Use this skill when the user asks about API metadata, base URLs, or parameters for any of these Microsoft Graph APIs:

- `/v1.0/users`
- `/v1.0/servicePrincipals`
- `/v1.0/identity/conditionalAccess/policies`

The supported CLI API names are:

```txt
users
service-principals
conditional-access-policies
```

## Commands

Run commands from the project root.

List supported APIs:

```bash
./graphmeta list
```

If the executable is inside the build directory, use:

```bash
./build/graphmeta list
```

Get metadata for the users API:

```bash
./graphmeta users
```

or:

```bash
./build/graphmeta users
```

Get metadata for the service principals API:

```bash
./graphmeta service-principals
```

or:

```bash
./build/graphmeta service-principals
```

Get metadata for the conditional access policies API:

```bash
./graphmeta conditional-access-policies
```

or:

```bash
./build/graphmeta conditional-access-policies
```

## Output Format

The CLI prints JSON.

Important fields:

```txt
apiName  - the supported API name
baseUrl  - the Microsoft Graph base URL
params   - the API parameters extracted from the OpenAPI data
```

The output may also include `method` and `path` for context.

Example CLI output:

```json
{
    "apiName": "users",
    "baseUrl": "https://graph.microsoft.com/v1.0",
    "method": "GET",
    "path": "/users",
    "params": [
        {
            "name": "$top",
            "in": "query",
            "required": false,
            "type": "integer"
        }
    ]
}
```
you should output in this format to user:
```txt
API Name: <api-name>
  Method: <HTTP-method>
  Base URL: <base-url>
  Path: <path>

  Params listed by the CLI:
  - <param-name>: <required/optional> <location>, <type> <optional short description>
```
Rules:
you can only get the information from the executable, do not inspect raw json or yaml file. do not inspect source code for informations
do not invent params that doesn't exists
output <optional short description> only if the param has a description in the CLI output. If not, omit it.
## Workflow

When the user asks for metadata about one of the supported APIs:

1. Map the user's request to one supported CLI API name.
2. Run the corresponding `graphmeta` command.
3. Read the JSON output.
4. Report the API Name, Base URL, and Params.
5. Do not guess fields that are not present in the CLI output.

## Response Style

Summarize the result clearly.

Example response:

```txt
API Name: users
Base URL: https://graph.microsoft.com/v1.0
Params:
- $top: query parameter, integer, optional
```

If `params` is empty, say:

```txt
Params: none listed by the CLI output.
```

## Examples

### Users API

User asks:

```txt
What parameters does the users API support?
```

Run:

```bash
./graphmeta users
```

Then summarize `apiName`, `baseUrl`, and `params`.

### Service Principals API

User asks:

```txt
Get the API metadata for service principals.
```

Run:

```bash
./graphmeta service-principals
```

Then summarize `apiName`, `baseUrl`, and `params`.

### Conditional Access Policies API

User asks:

```txt
Show the API info for conditional access policies.
```

Run:

```bash
./graphmeta conditional-access-policies
```

Then summarize `apiName`, `baseUrl`, and `params`.
For the 'params' field, list each parameter's name, location (query/path/header), type, and whether it is required.
## Rules

Do not send real Microsoft Graph API requests.

Do not ask the user for OAuth tokens or credentials.

Do not modify Microsoft 365, Google Workspace, or Microsoft Entra resources.

Do not guess parameters that are not shown in the CLI output.

If the user asks for an unsupported API, run:

```bash
./graphmeta list
```

Then explain which APIs are supported.

## Error Handling

If `./graphmeta` does not exist, try:

```bash
./build/graphmeta
```

If the CLI reports `Unknown API`, run:

```bash
./graphmeta list
```

and choose one of the supported API names.

If the CLI cannot find its data file, run the command from the project root and check that the `data/` folder exists.


## Bundled Runtime Library Fallback

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
