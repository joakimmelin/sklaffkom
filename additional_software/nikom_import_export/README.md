# NiKom migration

Copy the NiKom data volume from the Amiga to the Sklaffkom computer
Stop NiKom before running this, then copy the complete
data root containing
`DatoCfg`, `Users`, and `Moten`, at `~/backup`, then run:

```sh
python3 ../NiKom/Tools/NiKomExport/nikom_export.py
```

This reads `~/backup` and writes `~/code/NikomExport.dat`. Pass the data root and output path as positional arguments to override those defaults.

The reader expects the original big-endian NiKom files and stops on truncated
records. It does not modify the NiKom data.

Disable all Sklaffkom logins and then run: 

```sh
python3 tools/nikom_import.py NiKomExport.dat --sklaffdir /usr/local/sklaff
```

Use `--dry-run` first. The importer uses an exclusive lock and a source-record
ledger. User and conference names are matched case-insensitively. New users
receive synthetic Sklaff IDs and mailbox directories; they are application
records, and a Rocky Linux account whose username is the first name, full
name is the GECOS value, password is copied from NiKom, and shell is
`/usr/local/bin/sklaffkom`. Run as root. Existing users, conferences, letters,
posts, and memberships are never replaced. NiKom conference memberships are added to each imported user's `confs` file. Existing conference metadata is updated only to advance its last-post counter. Use `--skip-system-accounts` only for tests.

Make sure all Sklaffkom database files are owned by user and group sklaff. 

