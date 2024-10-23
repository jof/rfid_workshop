#!/usr/bin/env python
import re
import json
from argparse import ArgumentParser
from dataclasses import dataclass, asdict

import pm3
from textual.app import App, ComposeResult
from textual.containers import Container, Horizontal, ScrollableContainer
from textual.screen import Screen
from textual.widgets import Button, Footer, Header, DataTable, Static, Input
from textual.reactive import reactive
import random
from typing import List


@dataclass
class EnrolledCard:
    name: str
    facility_code: int
    card_number: int


class Database:
    def __init__(self, filename: str = "enrolled_cards.json"):
        self.filename = filename
        self.cards: List[EnrolledCard] = []
        self.load()

    def load(self):
        try:
            with open(self.filename, "r") as f:
                data = json.load(f)
                self.cards = [EnrolledCard(**card) for card in data]
        except FileNotFoundError:
            self.cards = []

    def save(self):
        with open(self.filename, "w") as f:
            json.dump([asdict(card) for card in self.cards], f, indent=2)

    def add_card(self, card: EnrolledCard):
        self.cards.append(card)
        self.save()


class EnrollmentScreen(Screen):
    def compose(self) -> ComposeResult:
        yield Container(
            ScrollableContainer(
                DataTable(id="enrolled_cards"),
                id="enrolled_cards_container",
            ),
            Horizontal(Static("Name: ", classes="label"), Input(id="name")),
            Horizontal(
                Static("Facility Code: ", classes="label"),
                Input(id="facility_code"),
                Button("Random", classes="random", id="random_facility_code"),
            ),
            Horizontal(
                Static("Card Number: ", classes="label"),
                Input(id="card_number"),
                Button("Random", classes="random", id="random_card_number"),
            ),
            Horizontal(
                Button("Enroll Card", id="enroll_card", variant="primary"),
                Button("Main Menu", id="main_menu", variant="primary"),
            ),
            Static("", id="status", classes="success"),
        )

    def on_mount(self):
        self.update_enrolled_cards()

    def update_enrolled_cards(self):
        table = self.query_one("#enrolled_cards", DataTable)
        table.clear(columns=True)
        table.add_columns("Name", "Facility Code", "Card Number")
        for card in self.app.database.cards:
            table.add_row(card.name, str(card.facility_code), str(card.card_number))

    def enroll_card(self):
        name = self.query_one("#name", Input).value
        facility_code = int(self.query_one("#facility_code", Input).value)
        card_number = int(self.query_one("#card_number", Input).value)

        success = write_hid26_to_t5577(facility_code, card_number)

        if success:
            card = EnrolledCard(name, facility_code, card_number)
            self.app.database.add_card(card)
            self.update_status("Card written successfully!")
            self.update_enrolled_cards()
        else:
            self.update_status("Failed to write card. Please try again.", "error")

        # Clear input fields
        self.query_one("#name", Input).value = ""
        # self.query_one("#facility_code", Input).value = ""
        self.query_one("#card_number", Input).value = ""

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "enroll_card":
            self.enroll_card()
        elif event.button.id == "random_facility_code":
            self.query_one("#facility_code", Input).value = str(random.randint(1, 255))
        elif event.button.id == "random_card_number":
            self.query_one("#card_number", Input).value = str(random.randint(1, 65535))
        elif event.button.id == "main_menu":
            self.app.pop_screen()

    def update_status(self, status: str, status_type: str = "success"):
        widget = self.query_one("#status", Static)
        widget.update(status)
        widget.remove_class("success")
        widget.remove_class("error")
        widget.add_class(status_type)


class ReaderScreen(Screen):
    pass


class MenuApp(App):
    CSS = """
    #buttons {
        align: center middle;
        width: 100%;
    }
    Button {
        width: 75%;
        margin: 1 2;
    }
    Container {
        padding: 1 2;
    }
    Horizontal {
        margin: 1 2;
    }
    .label {
        width: 25%;
        padding: 1 0;
    }
    Input {
        width: 25%;
        margin: 1 2;
    }
    Button.random {
        width: 25%;
        margin: 1 2;
    }
    #status {
        width: 100%;
        height: 3;
        content-align: center middle;
    }
    #status.success {
        background: $success;
        color: $text;
    }
    #status.error {
        background: $error;
        color: $text;
    }
    #enroll_card  {
        width: 50%;
    }
    #main_menu {
        width: 50%;
    }
    """
    BINDINGS = [("q", "quit", "Quit")]

    def __init__(self):
        super().__init__()
        self.database = Database()

    def compose(self) -> ComposeResult:
        yield Header()
        yield Footer()
        yield Container(
            Button(label="Enrollment Mode", variant="primary", id="enrollment"),
            Button(label="Reader Mode", variant="primary", id="reader"),
            id="buttons",
        )

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "enrollment":
            self.push_screen(EnrollmentScreen())
        elif event.button.id == "reader":
            self.push_screen(ReaderScreen())

    def on_quit(self) -> None:
        self.database.save()


def main():
    parser = ArgumentParser(description="Reader App")
    parser.add_argument(
        "--port", "-p", default="/dev/ttyACM1", help="Serial port to use"
    )
    args = parser.parse_args()

    global proxmark3
    proxmark3 = pm3.pm3(args.port)
    if proxmark3.console("hw version") != 0:
        print("Failed to connect to Proxmark3")
        return

    app = MenuApp()
    app.run()


def write_hid26_to_t5577(facility_code: int, card_number: int) -> bool:
    result = proxmark3.console("lf t55 detect")
    output = proxmark3.grabbed_output
    if result != 0:
        return False

    result = proxmark3.console(
        f"lf hid clone -w H10301 --fc {facility_code} --cn {card_number}"
    )
    output = proxmark3.grabbed_output
    if result != 0:
        return False

    result = proxmark3.console("lf hid read")
    if result != 0:
        return False
    # [+] [H10301  ] HID H10301 26-bit                FC: 10  CN: 10  parity ( ok )
    # [+] [ind26   ] Indala 26-bit                    FC: 160  CN: 10  parity ( ok )
    # [=] found 2 matching formats
    # [+] DemodBuffer:
    # [+] 1D5559555565566555555666
    # [=] raw: 000000000000002004140015
    output = proxmark3.grabbed_output
    successful_regex = (
        rf"FC:\s+{facility_code}\s+CN:\s+{card_number}\s+parity\s+\( ok \)"
    )
    if not re.search(successful_regex, output):
        return False

    return True


if __name__ == "__main__":
    main()
