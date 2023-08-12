#!/usr/bin/env python3
import os
import re
import subprocess
import sys
import tempfile
from hashlib import sha256
from typing import List, Tuple
import json

from secrets import CAMP_SECRET_FLAG, KEY_DIVERSIFICATION_SECRET

# Enroll a bunch of blank Mifare Classic 1K keys with diversified keys and content
# In a loop:
# - Wait to read a key to start
# - `hf mf autopwn` to a dump file
# - Capture UID
# - Compute per-sector-key diversified keys
# - Load existing dump file contents into buffer
# - Modify buffer with diversified keys
# - Insert the Flag-string into Sector 1, Block 4
# - Insert some non-zero data into Sectors 2 - 15

def pm3_autopwn() -> Tuple[str, str, str]:
    output = subprocess.run(
        f"env -S pm3",
        input=bytes("hf mf autopwn", encoding="utf-8"),
        shell=True,
        stdout=subprocess.PIPE,
    )
    output.check_returncode()
    output_lines = output.stdout.decode().split("\n")
    json_dump_file_pattern = r"saved to json file (\S+.json)"
    key_file_pattern = r"Found keys have been dumped to (\S+.bin)"
    json_output_file_line = next(
        line for line in output_lines if re.search(json_dump_file_pattern, line)
    )
    key_file_line = next(
        line for line in output_lines if re.search(key_file_pattern, line)
    )
    if not json_output_file_line:
        raise RuntimeError("No JSON dump file generated")
    if not key_file_pattern:
        raise RuntimeError("No Key file created")
    json_output_file = re.search(
        json_dump_file_pattern, json_output_file_line
    ).groups()[0]
    key_file = re.search(key_file_pattern, key_file_line).groups()[0]
    with open(json_output_file, "r") as json_file:
        json_doc = json.load(json_file)
    return (json_doc["Card"]["UID"], json_output_file, key_file)


def mifare_key_hash(b: bytes) -> bytes:
    # Diffuse some input values with a hashing function
    # SHA256, slicing off the first 6 bytes of the output digest (to match the
    #   MIFARE Classic key length)
    sum = sha256()
    sum.update(b)
    return sum.digest()[0:6]


def diversify_keys(uid: bytes) -> List[Tuple[bytes, bytes]]:
    sector_keys: List[Tuple] = []
    for sector in range(0, 16):
        sector_byte = bytes(sector)
        key_a = bytes("a", encoding="utf-8")
        key_b = bytes("b", encoding="utf-8")
        sector_keys.append(
            (
                mifare_key_hash(uid + sector_byte + key_a),
                mifare_key_hash(uid + sector_byte + key_b),
            )
        )
    return sector_keys


def pm3_restore(uid, dump_file, key_file) -> None:
    output = subprocess.run(
        f"env -S pm3",
        input=bytes(
            f"hf mf restore --1k --uid {uid} --kfn {key_file} --file {dump_file} --ka",
            encoding="utf-8",
        ),
        shell=True,
        stdout=subprocess.PIPE,
    )
    # print(output.stdout.decode())
    return output


if __name__ == "__main__":
    # FIXME
    os.environ["PATH"] = f"/Users/jof/src/proxmark3:{os.environ['PATH']}"

    # temporary_directory = tempfile.mkdtemp()
    temporary_directory = "/tmp"
    print(f"Temp Directory: {temporary_directory}")
    os.chdir(temporary_directory)

    while True:
        print("# Place blank tag and press Return/Enter")
        sys.stdin.readline()
        uid, dump_file, key_file = pm3_autopwn()
        sector_keys = diversify_keys(bytes.fromhex(uid))
        with open(dump_file, "rb") as file:
            json_doc = json.load(file)
        print(f"# Loaded JSON Dump file for UID {uid}")

        print(f"# Setting diversified keys in the JSON doc...")
        # This skips Sector 0, as we still want to have a default-keyed sector
        # for nested attacks
        for sector in range(1, 16):
            key_a = sector_keys[sector][0].hex().upper()
            key_b = sector_keys[sector][1].hex().upper()
            json_doc["SectorKeys"][str(sector)]["KeyA"] = key_a
            json_doc["SectorKeys"][str(sector)]["KeyB"] = key_b
            sector_trailer_block_number = ((sector + 1) * 4) - 1
            old_sector_trailer_block = json_doc["blocks"][
                str(sector_trailer_block_number)
            ]
            new_sector_trailer_block = key_a + old_sector_trailer_block[12:20] + key_b
            json_doc["blocks"][
                str(sector_trailer_block_number)
            ] = new_sector_trailer_block

        print("Inserting the secret flag into Sector 1, Block 4")
        json_doc["blocks"]["4"] = CAMP_SECRET_FLAG

        print("# Writing new JSON Dump file")
        new_dump_file = dump_file.replace(".json", ".new.json")
        with open(new_dump_file, "x") as file:
            json.dump(json_doc, file, indent=2)

        print("# Writing new card contents with new JSON Dump File...")
        # pm3_restore(uid, new_dump_file, key_file)

        print(f"# Done personalizing UID {uid}")
        break
