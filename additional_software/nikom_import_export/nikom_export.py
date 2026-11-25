#!/usr/bin/env python3
"""Export a NiKom data volume directly into NIKOM-MIGRATION-1 format.

This reader intentionally understands the raw Amiga files rather than using
the NiKom runtime.  NiKom's persisted integers are big-endian.
"""

import argparse
import base64
import os
import re
import struct
import sys
import unicodedata


SYSINFO = struct.Struct(">12l2h")
USER_NAME_OFFSET = 18 * 4
USER_PASSWORD_OFFSET = USER_NAME_OFFSET + 41 * 4 + 21 + 61
USER_MOTMED_OFFSET = USER_PASSWORD_OFFSET + 16 + 2 + 1 + 6 + (2048 // 8)
MOTE_RECORD = struct.Struct(">8x14l4hB41s70s50s80s")
# Head files in the persisted NiKom format use a 38-byte subject field.
HEADER_RECORD = struct.Struct(">3h39l38s")
HEADER_SIZE = HEADER_RECORD.size


def fail(message):
    raise RuntimeError(message)


def b64(value):
    return base64.b64encode(value).decode("ascii")


def c_string(value):
    return value.split(b"\0", 1)[0]


def normalized(value):
    value = unicodedata.normalize("NFKD", value)
    return "".join(c for c in value if not unicodedata.combining(c)).casefold()


def find_file(directory, names):
    wanted = {normalized(name) for name in names}
    for name in os.listdir(directory):
        if normalized(name) in wanted:
            return os.path.join(directory, name)
    fail("cannot find " + " or ".join(names) + " in " + directory)


def find_directory(directory, names):
    wanted = {normalized(name) for name in names}
    for entry in os.scandir(directory):
        if entry.is_dir() and normalized(entry.name) in wanted:
            return entry.path
    fail("cannot find " + " or ".join(names) + " in " + directory)


def read_exact(file, size, description):
    data = file.read(size)
    if len(data) != size:
        fail("truncated " + description)
    return data


def field(value):
    return "|" + b64(value)


def memberships(data):
    bitmap = data[USER_MOTMED_OFFSET:USER_MOTMED_OFFSET + (2048 // 8)]
    ids = []
    for conference in range(1, 2048):
        if bitmap[conference // 8] & (1 << (7 - (conference % 8))):
            ids.append(str(conference).encode("ascii"))
    return b",".join(ids)


def iter_users(root):
    users = find_directory(root, ("Users",))
    entries = []
    for bucket in os.scandir(users):
        if not bucket.is_dir() or not bucket.name.isdigit():
            continue
        for user in os.scandir(bucket.path):
            if not user.is_dir() or not user.name.isdigit():
                continue
            user_id = int(user.name)
            data_path = os.path.join(user.path, "Data")
            with open(data_path, "rb") as file:
                data = file.read()
            if len(data) < USER_PASSWORD_OFFSET + 16:
                fail("truncated " + data_path)
            name = c_string(data[USER_NAME_OFFSET:USER_NAME_OFFSET + 41])
            if not name:
                continue
            last_session = struct.unpack_from(">l", data, 8)[0]
            password = c_string(data[USER_PASSWORD_OFFSET:USER_PASSWORD_OFFSET + 16])
            entries.append((user_id, last_session, name, password, memberships(data), user.path))
    return sorted(entries)


LETTER_RE = re.compile(r"^(\d+)\.letter$")


def letters(user_id, user_path):
    for entry in sorted(os.scandir(user_path), key=lambda item: item.name):
        match = LETTER_RE.match(entry.name)
        if not match or not entry.is_file():
            continue
        values = {b"System-ID": b"", b"From": b"", b"To": b"", b"Reply": b"",
                  b"Date": b"", b"Subject": b""}
        body = []
        in_body = False
        with open(entry.path, "rb") as file:
            for raw in file:
                line = raw.rstrip(b"\r\n")
                if not in_body:
                    matched = False
                    for key in values:
                        prefix = key + b":"
                        if line.startswith(prefix):
                            values[key] = line[len(prefix):].lstrip(b" ")
                            matched = True
                            if key == b"Subject":
                                in_body = True
                            break
                    if matched:
                        continue
                body.append(line + b"\n")
        yield (user_id, int(match.group(1)), values, b"".join(body))


def conferences(root):
    data_dir = find_directory(root, ("DatoCfg",))
    path = find_file(data_dir, ("Möten.dat", "Moten.dat"))
    with open(path, "rb") as file:
        number = 0
        while True:
            data = file.read(MOTE_RECORD.size)
            if not data:
                break
            if len(data) != MOTE_RECORD.size:
                fail("truncated " + path)
            values = MOTE_RECORD.unpack(data)
            name = c_string(values[19])
            if name:
                yield number, values[0], values[18], name
            number += 1


def sysinfo_and_conference_texts(root):
    data_dir = find_directory(root, ("DatoCfg",))
    info_path = find_file(data_dir, ("Sysinfo.dat",))
    with open(info_path, "rb") as file:
        info = SYSINFO.unpack(read_exact(file, SYSINFO.size, info_path))
    low, high = info[1], info[0]
    if high < low:
        return low, high, {}
    texts_path = find_file(data_dir, ("ConferenceTexts.dat",))
    with open(texts_path, "rb") as file:
        raw = read_exact(file, (high - low + 1) * 2, texts_path)
    conferences_by_text = dict(zip(range(low, high + 1), struct.unpack(">%dh" % (high - low + 1), raw)))
    return low, high, conferences_by_text


def post_records(root):
    low, high, conference_by_text = sysinfo_and_conference_texts(root)
    if high < low:
        return
    moten = find_directory(root, ("Moten",))
    for file_number in range(low // 512, high // 512 + 1):
        head_path = os.path.join(moten, "Head%d.dat" % file_number)
        if not os.path.exists(head_path):
            continue
        with open(head_path, "rb") as heads:
            while True:
                raw = heads.read(HEADER_SIZE)
                if not raw:
                    break
                if len(raw) != HEADER_SIZE:
                    fail("truncated " + head_path)
                values = HEADER_RECORD.unpack(raw[:HEADER_RECORD.size])
                number = values[20]
                if not low <= number <= high:
                    continue
                conference = conference_by_text[number]
                if conference < 0:
                    continue
                line_count = values[0]
                text_path = os.path.join(moten, "Text%d.dat" % (number // 512))
                with open(text_path, "rb") as text_file:
                    text_file.seek(values[21])
                    body_lines = []
                    for _ in range(line_count):
                        line = text_file.readline()
                        if not line:
                            fail("truncated text body for post %d" % number)
                        body_lines.append(line.rstrip(b"\r\n") + b"\n")
                reply = values[38]
                reply_conference = conference_by_text.get(reply, -1)
                yield (conference, number, values[3], values[22], reply,
                       values[19], reply_conference, c_string(values[42]), b"".join(body_lines))


def export(root, output):
    with open(output, "w", encoding="ascii", newline="\n") as out:
        out.write("NIKOM-MIGRATION-1\n")
        users = list(iter_users(root))
        for user_id, last_session, name, password, member_ids, user_path in users:
            out.write("U|%d|%d%s%s%s\n" %
                      (user_id, last_session, field(name), field(password), field(member_ids)))
            for owner, number, values, body in letters(user_id, user_path):
                fields = [values[key] for key in (b"System-ID", b"From", b"To", b"Reply", b"Date", b"Subject")]
                out.write("M|%d|%d%s\n" % (owner, number, "".join(field(value) for value in fields) + field(body)))
        for number, created, kind, name in conferences(root):
            out.write("C|%d|%d|%d%s\n" % (number, created, kind, field(name)))
        for conference, number, author, timestamp, reply, reply_author, reply_conference, subject, body in post_records(root):
            out.write("P|%d|%d|%d|%d|%d|%d|%d%s\n" % (
                conference, number, author, timestamp, reply, reply_author,
                reply_conference, field(subject) + field(body)))
    os.chmod(output, 0o600)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", default=os.path.expanduser("~/backup"),
                        help="NiKom data root (default: ~/backup)")
    parser.add_argument("output", nargs="?", default=os.path.expanduser("~/code/NikomExport.dat"),
                        help="migration file to create (default: ~/code/NikomExport.dat)")
    args = parser.parse_args()
    root = os.path.abspath(args.root)
    try:
        find_directory(root, ("DatoCfg",))
    except RuntimeError:
        nested = os.path.join(root, "NiKom")
        if os.path.isdir(nested):
            root = nested
    try:
        export(root, args.output)
    except (OSError, RuntimeError, struct.error) as error:
        print("nikom_export: %s" % error, file=sys.stderr)
        return 10
    return 0


if __name__ == "__main__":
    sys.exit(main())
