# Importing OpenSSH hosts

This feature imports endpoint definitions from a local OpenSSH client configuration. It creates ordinary NetWatch **TCP checks**, not authenticated SSH sessions. A successful check means the configured port accepts a TCP connection; it does not verify the SSH banner, host key, credentials or remote command execution.

## Workflow

1. Open NetWatch settings and click **Import SSH...**.
2. Choose the file. The initial suggestion is `%USERPROFILE%\.ssh\config`; the source file is never modified.
3. Inspect the preview: Host alias, resolved destination, port and import status. Only ready rows are selected initially. Uncheck unwanted rows.
4. Select a destination group. An existing group labeled SSH is selected initially; otherwise a new SSH group is offered. Other existing groups are available too.
5. Click **Add selected**. Review the TCP checks in the settings draft. Defaults are the normal 30-second interval and 2000-ms timeout.
6. **Save** persists the draft and applies monitoring changes. **Cancel** in the preview or main settings does not apply the import. A new visible group requires a host restart to export its indicator; adding checks to an already exported group does not.

If the destination already contains a TCP check with the same alias or case-insensitively equal address and port, that row is marked as a duplicate. Existing checks, settings and UUIDs are preserved, including when an alias now points somewhere else. Edit or remove the existing check explicitly to adopt a changed destination. Duplicate endpoints within the source are also reduced to their first ready alias. Different destination groups remain independent.

## Supported configuration

The interpretation follows the [OpenSSH ssh_config manual](https://man.openbsd.org/ssh_config): first obtained values win, Host patterns can supply defaults, and relative user Include paths are anchored in the user's `.ssh` directory. This is a deliberately limited reader, not a complete replacement for OpenSSH's configuration engine.

- UTF-8, an optional UTF-8 BOM, LF/CRLF, blank lines, comments outside quotes, quoted values and keyword/value separators using whitespace or `=`.
- Case-insensitive keywords; case-sensitive Host aliases/patterns. Only literal positive aliases enumerate endpoints. `*`, `?` and negated patterns determine which options apply but cannot discover host names.
- `HostName` and `Port`, including defaults from matching sections. An absent HostName uses the alias; an absent Port uses 22. Multiple aliases on one Host line are supported. IPv4/IPv6 literals are accepted.
- HostName `%h` and `%%`; other percent tokens and environment expansions are skipped with an explanation. Unsupported/invalid names and ports are not imported.
- Local `Include` paths, `~/` for the current Windows profile, and `*`/`?` in the file-name component. Matches are read in lexical order. Relative paths use `%USERPROFILE%\.ssh`, even when a custom top-level config file is selected. Each included file inherits the containing Host condition and restores it on return, including between glob matches.

For example, the following user-selected source produces a TCP check named `lab` targeting the documentation address `192.0.2.10`, port 2222. This example is not installed as a default target:

```sshconfig
Host lab
    HostName 192.0.2.10
    Port 2222

Host *
    Port 22
```

## Limits and exclusions

- `Match` rejects the import with a diagnostic. No `Match exec`, `ProxyCommand`, `LocalCommand`, remote command, shell or `ssh -G` is executed by the plugin.
- Aliases requiring ProxyJump/ProxyCommand or hostname canonicalization are skipped. This version does not follow jump hosts or tunnels.
- Authentication, User/IdentityFile, forwarding, bind/address-family preferences and the system-wide SSH configuration are not applied to TCP checks. IdentityFile paths are not opened for authentication; the reader only follows explicitly referenced Include configuration files.
- Include environment/percent expansions, other users' tilde paths and wildcard directory components are rejected. Missing literal Include files also reject the import, conservatively avoiding a partial result; an unmatched file-name wildcard contributes no files. UNC paths and remote drives are rejected. File chooser cancellation leaves the draft unchanged apart from the editor's usual field capture.
- Resource limits: 64 file reads, Include depth at most 8 below the top-level file, 16 MiB of input, 16 KiB per line, 65,536 records, 16,384 literal aliases and four million evaluation steps. Cycles, malformed quotes and limit violations stop the import before changing the draft. Include files are parsed for syntax even when their Host condition is inactive; unsupported Match syntax there is still rejected.
- The source is a one-time snapshot. Later SSH-config changes do not silently synchronize or overwrite checks. No private source file, alias list or destination is copied into this repository by the import feature.

Configuration reads and preview preparation happen only through the explicit settings action, outside the engine mutex. They perform no probe/network work. Imported checks use the existing shared worker and launch limits after Save. Original implementation and tests are in `src/ssh_import.cpp` and `tests/ssh_tests.cpp`; parsing behavior was checked against the manual and the upstream [Include implementation](https://github.com/openssh/openssh-portable/blob/master/readconf.c).
