# RFID Hacking Workshop

Here are materials for my RFID Hacking Workshop, first presented at Chaos Communication Camp 2023.

The goal of this workshop is to introduce people from all backgrounds to common RFID technologies, provide some inexpensive hardware to interact with tags, and present some vulnerabilities in the common Mifare Classic tag type.

Participants are given a Mifare Classic fob with diversified and unknown keys, are shown how to crack the keys and clone the fob contents onto an empty card.

Additionally, some readers based on InstaNFC hardware are present so that participants can test their clones and get audio-visual feedback on success or failure.

## `key_creator/`

`key_creator/` contains scripts used to quickly program fobs (using Proxmark3) in an easy mode (only one sector is protected, all others with default keys), or hard mode (all sectors protected with diversified keys).

## `reader/`

`reader/` contains a PlatformIO project, targeting InstaNFC hardware running on ESP32, which scans candidate cloned cards and provides audio-visual feedback on success or failure.
