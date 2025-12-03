# User Management Service

Overview:
- Provides in-memory user accounts and a superuser (`root`, UID 0)
- Minimal authentication using FNV-1a hashing (development only)
- Shell integrates user context in prompt and commands

Service:
- Name: `USER`
- Starts at boot and bootstraps `root` (password: `root`) and `user` (password: `user`)
- APIs: add/delete user, set password, authenticate, login/logout, query current user

Shell Commands:
- `whoami` — shows current user
- `su <user> <password>` — login as user
- `adduser <user> <password>` — add user (root only)
- `passwd <user> <newpass>` — change password (root or the target user)
- `reboot`, `shutdown` — guarded; require root

Prompt:
- Displays `user@kos:<path>$` or `root@kos:<path>#` (root uses `#`)

Notes:
- Password hashing is non-cryptographic; replace with stronger hashing when crypto is available.
- Persistence is not implemented yet; accounts reset on reboot. Future work: persist to `/etc/passwd`-like file.
