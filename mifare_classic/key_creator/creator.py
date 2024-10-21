#!/usr/bin/env python3
import os
import re
import subprocess
import sys
import json
from typing import Tuple

from camp_secrets import CAMP_SECRET_FLAG
from keys import diversify_keys, key_file_binary

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
        stderr=subprocess.DEVNULL,
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




def pm3_restore(uid, dump_file, key_file) -> None:
    output = subprocess.run(
        f"env -S pm3",
        input=bytes(
            f"hf mf restore --1k --uid {uid} --kfn {key_file} --file {dump_file} --ka",
            encoding="utf-8",
        ),
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
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
    hard_mode = False

    while True:
        print("# Place blank tag and press \"e\"+Enter for easy mode, or \"h\"+Enter for hard mode")
        input_line = sys.stdin.readline().strip().lower()
        if input_line == 'e':
            hard_mode = False
            print("# Easy Mode")
        elif input_line == 'h':
            hard_mode = True
            print("# Hard Mode")
        else:
            continue
        uid, dump_file, key_file = pm3_autopwn()
        sector_keys = diversify_keys(bytes.fromhex(uid))
        with open(dump_file, "rb") as file:
            json_doc = json.load(file)
        print(f"# Loaded JSON Dump file for UID {uid}")

        print(f"# Setting diversified keys in the JSON doc...")
        if hard_mode:
            key_range = (1, 16)
        else:
            key_range = (1, 2)
        for sector in range(*key_range):
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

        if hard_mode:
            print("## Setting the \"hard mode\" flag into Sector 1, Block 4")
            json_doc["blocks"]["4"] = '11000000000000000000000000000000'
        else:
            print("## Setting the \"easy mode\" flag into Sector 1, Block 4")
            json_doc["blocks"]["4"] = 'FF000000000000000000000000000000'

        print("## Inserting the secret flag into Sector 1, Block 5")
        json_doc["blocks"]["5"] = CAMP_SECRET_FLAG

        if hard_mode:
            print("## Setting \"hard mode \" flags into Blocks 6 - 63")
            for block in range(6, 64):
                if block % 4 == 3:
                    # Skip Sector trailer blocks
                    continue
                block_byte = int.to_bytes(block, 1, byteorder='big')
                block_flag = block_byte * 16
                json_doc["blocks"][str(block)] = block_flag.hex().upper()

        new_dump_file = dump_file.replace(".json", ".new.json")
        print(f"# Writing new JSON Dump file: {new_dump_file}")
        with open(new_dump_file, "x") as file:
            json.dump(json_doc, file, indent=2)

        new_key_file = key_file.replace("-key", "-key.new")
        print(f"# Writing new key file {new_key_file}")
        with open(new_key_file, "wb") as file:
            file.write(key_file_binary(sector_keys, key_range))

        print("# Writing new card contents with new JSON Dump File...")
        pm3_restore(uid, new_dump_file, key_file)

        print(f"# Done personalizing UID {uid}")
