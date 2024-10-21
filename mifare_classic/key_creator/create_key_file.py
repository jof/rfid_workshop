#!/usr/bin/env python3
import os
import re
import subprocess
import sys

from keys import diversify_keys, key_file_binary


def pm3_get_uid() -> bytes:
    output = subprocess.run(
        f"env -S pm3",
        input=bytes("hf 14a reader", encoding="utf-8"),
        shell=True,
        stdout=subprocess.PIPE,
    )
    output.check_returncode()
    output_lines = output.stdout.decode().split("\n")
    uid_pattern = r"UID: ((?:[0-9a-fA-F]{2}\s?){4})"
    uid_line = next(line for line in output_lines if re.search(uid_pattern, line))
    if not uid_line:
        raise RuntimeError("No 4-byte UID found")
    uid = ''.join(re.search(uid_pattern, uid_line).groups()[0].split())
    uid = bytes.fromhex(uid)
    return uid


if __name__ == "__main__":
    # FIXME
    os.environ["PATH"] = f"/Users/jof/src/proxmark3:{os.environ['PATH']}"

    # temporary_directory = tempfile.mkdtemp()
    temporary_directory = "/tmp"
    print(f"Temp Directory: {temporary_directory}")
    os.chdir(temporary_directory)

    while True:
        print("# Place tag to read UID and press Return/Enter")
        sys.stdin.readline()
        uid = pm3_get_uid()
        sector_keys = diversify_keys(uid)
        new_key_file = f"hf-mf-{uid.hex().upper()}.new.keys.bin"
        print(f"# Writing new key file: {new_key_file}")
        with open(new_key_file, "wb") as file:
            file.write(key_file_binary(sector_keys))
