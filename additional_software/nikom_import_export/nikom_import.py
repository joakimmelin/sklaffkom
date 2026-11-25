#!/usr/bin/env python3
"""Append-only importer for NiKomExport files into SklaffKOM."""

import argparse
import base64
import datetime
import fcntl
import grp
import os
import pwd
import re
import shutil
import subprocess
import tempfile
import time
import unicodedata


def b64(value):
    if isinstance(value, str):
        value = value.encode("ascii")
    return base64.b64decode(value)


def clean_name(value):
    return value.rstrip(b"\x00\r\n")


SF7_FROM_LATIN1 = str.maketrans({
    "å": "}", "ä": "{", "ö": "|",
    "Å": "]", "Ä": "[", "Ö": "\\",
})
SF7_TO_UNICODE = str.maketrans({
    "}": "å", "{": "ä", "|": "ö",
    "]": "Å", "[": "Ä", "\\": "Ö",
})


def source_text(value):
    """Convert NiKom Latin-1 text to Sklaffkom's default SF7 bytes."""
    value = clean_name(value)
    try:
        text = value.decode("utf-8")
    except UnicodeDecodeError:
        text = value.decode("latin-1")
    return text.translate(SF7_FROM_LATIN1).encode("latin-1")


def sf7_unicode(value):
    """Decode Sklaffkom SF7 bytes for Linux interfaces that require UTF-8."""
    return clean_name(value).decode("latin-1").translate(SF7_TO_UNICODE)


def post_subject(value):
    """Keep the subject as the single line required by get_text_entry()."""
    return source_text(value).replace(b"\x00", b"").replace(b"\r", b" ").replace(b"\n", b" ")


def post_body(value):
    """Return a newline-terminated body and its exact Sklaffkom line count."""
    body = source_text(value).replace(b"\x00", b"")
    if body and not body.endswith(b"\n"):
        body += b"\n"
    return body, body.count(b"\n")


def nikom_date(value):
    """Convert a NiKom letter Date header to SklaffKOM's Unix timestamp."""
    text = clean_name(value).decode("ascii", "replace").strip()
    for date_format in ("%d %b %y %H:%M:%S", "%d %b %y %H:%M",
                        "%y%m%d %H:%M:%S", "%y%m%d %H:%M",
                        "%d%m%y %H:%M:%S", "%d%m%y %H:%M"):
        try:
            parsed = datetime.datetime.strptime(text, date_format)
        except ValueError:
            continue
        return int(time.mktime(parsed.timetuple()))
    return 0


def split_lines(path):
    try:
        with open(path, "rb") as f:
            return f.read().splitlines(True)
    except FileNotFoundError:
        return []


def atomic_replace(path, data):
    directory = os.path.dirname(path) or "."
    fd, tmp = tempfile.mkstemp(prefix=".nikom-import-", dir=directory)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    finally:
        try:
            os.unlink(tmp)
        except FileNotFoundError:
            pass


def append_new(path, data):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
    try:
        os.write(fd, data)
        os.fsync(fd)
    finally:
        os.close(fd)


def ensure_dir(path):
    os.makedirs(path, mode=0o700, exist_ok=True)


def read_users(path):
    result = {}
    max_id = 0
    for raw in split_lines(path):
        line = raw.rstrip(b"\r\n")
        parts = line.split(b":", 2)
        if len(parts) != 3:
            continue
        try:
            uid = int(parts[0])
        except ValueError:
            continue
        result[source_text(parts[2]).lower()] = uid
        max_id = max(max_id, uid)
    return result, max_id


def linux_username(full_name):
    text = sf7_unicode(full_name)
    fields = text.split()
    if len(fields) >= 2:
        login = fields[0] + fields[-1]
    else:
        login = fields[0] if fields else text
    login = "".join(char for char in unicodedata.normalize("NFKD", login)
                    if not unicodedata.combining(char))
    login = login.encode("ascii", "ignore").decode("ascii")
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_.-]*", login):
        raise RuntimeError("cannot derive a valid Linux username from " + repr(full_name))
    return login


def system_account(username):
    """Return the passwd entry for username, or None when it is absent."""
    try:
        return pwd.getpwnam(username)
    except KeyError:
        return None


def create_linux_account(full_name, password):
    if os.geteuid() != 0:
        raise RuntimeError("Linux account creation requires root; run the importer as root")
    username = linux_username(full_name)
    if system_account(username) is not None:
        return username
    if b"\n" in password or b"\r" in password or b":" in password:
        raise RuntimeError("password for %r contains characters unsupported by chpasswd" % full_name)
    useradd = shutil.which("useradd") or "/usr/sbin/useradd"
    subprocess.run([useradd, "-m", "-c", sf7_unicode(full_name),
                    "-s", "/usr/local/bin/sklaffkom", username], check=True)
    if password.startswith((b"$", b"_", b".")) or len(password) == 13:
        subprocess.run(["/usr/sbin/usermod", "-p", password.decode("ascii"), username], check=True)
    else:
        subprocess.run(["/usr/sbin/chpasswd"], input=username.encode("ascii") + b":" + password + b"\n",
                       check=True)
    return username


def ensure_user(root, user_file, users, next_id, source_id, name, password, report, dry_run,
                skip_system_accounts):
    key = name.lower()
    if key in users:
        if not dry_run and not skip_system_accounts:
            username = linux_username(name)
            if system_account(username) is None:
                account = create_linux_account(name, password)
                report.append(f"created Linux account {account!r}")
        return users[key], next_id
    uid = max(next_id + 1, 1)
    users[key] = uid
    if not dry_run:
        append_new(user_file, (f"{uid}:0:".encode() + name + b"\n"))
        ensure_dir(os.path.join(root, "user", str(uid)))
        ensure_dir(os.path.join(root, "mbox", str(uid)))
        append_new(os.path.join(root, "user", str(uid), "confs"), b"0:\n")
        append_new(os.path.join(root, "user", str(uid), "sklaffrc"), b"")
        append_new(os.path.join(root, "mbox", str(uid), "mailbox"), b"0:0:0:0:0:14:0:Brevl}dan\n")
        if not skip_system_accounts:
            account = create_linux_account(name, password)
            report.append(f"created Linux account {account!r}")
    report.append(f"created user {name!r} as uid {uid}")
    return uid, uid


def parse_confs(path):
    result = {}
    max_id = 0
    for raw in split_lines(path):
        p = raw.rstrip(b"\r\n").split(b":", 7)
        if len(p) != 8:
            continue
        try:
            cid = int(p[0])
        except ValueError:
            continue
        result[source_text(p[7]).lower()] = (cid, p)
        max_id = max(max_id, cid)
    return result, max_id


def update_conf_last(path, cid, last):
    lines = split_lines(path)
    out = []
    changed = False
    for raw in lines:
        p = raw.rstrip(b"\r\n").split(b":", 7)
        if len(p) == 8 and int(p[0]) == cid:
            p[1] = str(last).encode()
            raw = b":".join(p) + b"\n"
            changed = True
        out.append(raw)
    if changed:
        atomic_replace(path, b"".join(out))


def existing_post_ids(root, cid):
    path = os.path.join(root, "db", str(cid))
    try:
        return {int(n) for n in os.listdir(path) if n.isdigit()}
    except FileNotFoundError:
        return set()


def ensure_conf(root, conf_file, confs, next_id, source_id, name, created, ctype, author, report, dry_run):
    key = name.lower()
    if key in confs:
        return confs[key][0], next_id
    cid = max(next_id + 1, 1)
    line = b":".join([str(cid).encode(), b"0", str(author).encode(), str(created).encode(), b"0", b"10", b"0", name]) + b"\n"
    if not dry_run:
        append_new(conf_file, line)
        ensure_dir(os.path.join(root, "db", str(cid)))
        append_new(os.path.join(root, "db", str(cid), "confrc"), b"")
    confs[key] = (cid, line.rstrip(b"\n").split(b":", 7))
    report.append(f"created conference {name!r} as {cid}")
    return cid, cid


def update_mailbox(root, uid, source_num, author, stamp, subject, body):
    directory = os.path.join(root, "mbox", str(uid))
    ensure_dir(directory)
    index = os.path.join(directory, "mailbox")
    lines = split_lines(index)
    last = 0
    for raw in lines:
        p = raw.rstrip(b"\r\n").split(b":", 7)
        if len(p) == 8 and p[0] == b"0":
            last = int(p[1])
            break
    new = last + 1
    body_lines = body.count(b"\n")
    post = f"{new}:{author}:{stamp}:0:0:0:{body_lines}:0\n".encode() + subject + b"\n" + body
    append_new(os.path.join(directory, str(new)), post)
    replaced = False
    out = []
    for raw in lines:
        p = raw.rstrip(b"\r\n").split(b":", 7)
        if len(p) == 8 and p[0] == b"0" and not replaced:
            p[1] = str(new).encode()
            raw = b":".join(p) + b"\n"
            replaced = True
        out.append(raw)
    if not replaced:
        out.append((f"0:{new}:0:0:0:14:0:".encode() + b"Brevl}dan\n"))
    atomic_replace(index, b"".join(out))


def ensure_memberships(root, uid, source_conferences, conf_map, report, dry_run):
    if not source_conferences:
        return
    directory = os.path.join(root, "user", str(uid))
    path = os.path.join(directory, "confs")
    existing = set()
    for raw in split_lines(path):
        try:
            existing.add(int(raw.split(b":", 1)[0]))
        except (ValueError, IndexError):
            continue
    additions = []
    for source_conf in source_conferences:
        destination = conf_map.get(source_conf)
        if destination is not None and destination not in existing:
            additions.append((destination, source_conf))
            existing.add(destination)
    if not additions:
        return
    if not dry_run:
        ensure_dir(directory)
        append_new(path, b"".join(str(destination).encode() + b":\n"
                                  for destination, unused in additions))
    report.append("added %d conference memberships for uid %d" %
                  (len(additions), uid))


def set_sklaff_owner(root):
    """Set ownership of every non-symlink in the Sklaffkom tree."""
    if os.geteuid() != 0:
        raise RuntimeError("setting Sklaffkom ownership requires root")
    try:
        owner = pwd.getpwnam("sklaff")
        group = grp.getgrnam("sklaff")
    except KeyError:
        raise RuntimeError("cannot find sklaff user and group")

    for directory, subdirectories, files in os.walk(root, followlinks=False):
        for name in subdirectories + files:
            path = os.path.join(directory, name)
            if not os.path.islink(path):
                os.chown(path, owner.pw_uid, group.gr_gid)
    if not os.path.islink(root):
        os.chown(root, owner.pw_uid, group.gr_gid)


def import_file(input_path, root, dry_run, skip_system_accounts):
    etc = os.path.join(root, "etc")
    user_file = os.path.join(etc, "user")
    conf_file = os.path.join(etc, "conf")
    ledger = os.path.join(root, "etc", ".nikom-imported")
    users, next_user = read_users(user_file)
    confs, next_conf = parse_confs(conf_file)
    user_map, conf_map, memberships = {}, {}, []
    posts = []
    report = []
    done = set(x.rstrip(b"\r\n") for x in split_lines(ledger))

    with open(input_path, "rb") as source:
        first = source.readline().rstrip(b"\r\n")
        if first != b"NIKOM-MIGRATION-1":
            raise SystemExit("unsupported migration file")
        for raw in source:
            p = raw.rstrip(b"\r\n").split(b"|")
            if not p or p[0] == b"":
                continue
            key = raw.rstrip(b"\r\n")
            if p[0] == b"U":
                password = clean_name(b64(p[4])) if len(p) > 4 else b""
                uid, next_user = ensure_user(root, user_file, users, next_user, int(p[1]),
                                             source_text(b64(p[3])), password, report, dry_run,
                                             skip_system_accounts)
                user_map[int(p[1])] = uid
                member_ids = b64(p[5]) if len(p) > 5 else b""
                source_conferences = []
                for value in member_ids.split(b","):
                    if value:
                        try:
                            source_conferences.append(int(value))
                        except ValueError:
                            continue
                memberships.append((int(p[1]), source_conferences))
            elif p[0] == b"C":
                name = source_text(b64(p[4]))
                cid, next_conf = ensure_conf(root, conf_file, confs, next_conf, int(p[1]), name, int(p[2]), int(p[3]), 0, report, dry_run)
                conf_map[int(p[1])] = cid
            elif p[0] == b"P":
                if key not in done:
                    posts.append((key, p))
            elif p[0] == b"M":
                if key in done:
                    continue
                owner = user_map.get(int(p[1]))
                if owner is None:
                    report.append(f"skipped letter {p[2]}: recipient user was not exported")
                    continue
                stamp = nikom_date(b64(p[7]))
                try:
                    author = int(clean_name(b64(p[4])))
                except ValueError:
                    author = 0
                subject = source_text(b64(p[8]))
                body = source_text(b64(p[9]))
                metadata = []
                for label, field in ((b"System-ID", p[3]), (b"From", p[4]),
                                     (b"To", p[5]), (b"Reply", p[6]),
                                     (b"Date", p[7])):
                    value = source_text(b64(field))
                    if value:
                        metadata.append(label + b": " + value + b"\n")
                if metadata:
                    body = b"".join(metadata) + b"\n" + body
                if not dry_run:
                    update_mailbox(root, owner, int(p[2]), user_map.get(author, 0), stamp, subject, body)
                append_new(ledger, key + b"\n") if not dry_run else None

    for source_user, source_conferences in memberships:
        uid = user_map.get(source_user)
        if uid is not None:
            ensure_memberships(root, uid, source_conferences, conf_map, report, dry_run)

    for key, p in posts:
        cid = conf_map.get(int(p[1]))
        if cid is None:
            report.append(f"skipped post {p[2]}: conference was not exported")
            continue
        author = user_map.get(int(p[3]), 0)
        existing = existing_post_ids(root, cid)
        num = max(existing or {0}) + 1
        comment = 0
        reply_conf = int(p[7])
        if int(p[5]) and (reply_conf, int(p[5])) in post_map:
            comment = post_map[(reply_conf, int(p[5]))]
        body, body_lines = post_body(b64(p[9]))
        subject = post_subject(b64(p[8]))
        post = f"{num}:{author}:{int(p[4])}:{comment}:{cid if comment else 0}:{user_map.get(int(p[6]), 0)}:{body_lines}:0\n".encode() + subject + b"\n" + body
        if not dry_run:
            append_new(os.path.join(root, "db", str(cid), str(num)), post)
            update_conf_last(conf_file, cid, num)
            append_new(ledger, key + b"\n")
        post_map[(int(p[1]), int(p[2]))] = num
    return report


post_map = {}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input")
    ap.add_argument("--sklaffdir", default="/usr/local/sklaff")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--skip-system-accounts", action="store_true",
                    help="do not create Linux login accounts (for testing only)")
    args = ap.parse_args()
    root = os.path.abspath(args.sklaffdir)
    if not args.dry_run:
        ensure_dir(os.path.join(root, "etc"))
    lock_path = os.path.join(root, "etc", ".nikom-import.lock")
    with open(lock_path, "a+b") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        for line in import_file(args.input, root, args.dry_run, args.skip_system_accounts):
            print(line)
        if not args.dry_run:
            set_sklaff_owner(root)


if __name__ == "__main__":
    main()
