from hashlib import sha256
from typing import Tuple, List

from camp_secrets import KEY_DIVERSIFICATION_SECRET


def mifare_key_hash(b: bytes) -> bytes:
    # Diffuse some input values with a hashing function
    # SHA256, slicing off the first 6 bytes of the output digest (to match the
    #   MIFARE Classic key length)
    sum = sha256()
    sum.update(b)
    digest = sum.digest()
    return sum.digest()[0:6]

def xor(input: bytes, secret: bytes) -> bytes:
    # XOR some equal-length byte strings together
    if len(input) != len(secret):
        raise ValueError

    if len(input) != 6:
        raise ValueError
    if len(secret) != 6:
        raise ValueError

    return bytes([x ^ y for x, y in zip (input, secret)])


def diversify_keys(uid: bytes) -> List[Tuple[bytes, bytes]]:
    sector_keys: List[Tuple] = []
    for sector in range(0, 16):
        sector_byte = int.to_bytes(sector, 1, byteorder='big')
        key_a = bytes("a", encoding="utf-8")
        key_b = bytes("b", encoding="utf-8")
        sector_keys.append(
            (
                xor(mifare_key_hash(uid + sector_byte + key_a), KEY_DIVERSIFICATION_SECRET),
                xor(mifare_key_hash(uid + sector_byte + key_b), KEY_DIVERSIFICATION_SECRET),
            )
        )
    return sector_keys

def key_file_binary(sector_keys: List[Tuple[bytes, bytes]], key_range: Tuple[int, int]) -> bytes:
    key_file = bytes()
    key_range = range(*key_range)
    # First, A Keys
    for sector_number, sector in enumerate(sector_keys):
        if sector_number in key_range:
            key_file = key_file + sector[0]
        else:
            key_file = key_file + bytes.fromhex('ffffffffffff')
    # Then, B Keys
    for sector_number, sector in enumerate(sector_keys):
        if sector_number in key_range:
            key_file = key_file + sector[1]
        else:
            key_file = key_file + bytes.fromhex('ffffffffffff')
    return key_file
