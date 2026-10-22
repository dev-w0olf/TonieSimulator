#!/usr/bin/env python3
"""
Generate a compact Tonies catalog file (assets/tonies_catalog.txt) for Flipper Zero
from toniebox-reverse-engineering/tonies-json using the directory and filename
normalization rules from nortakales/flipper-zero-tonies (figures_name_tool.pyw).

Format of assets/tonies_catalog.txt:
    <LANG>\t<TOP_SERIES_FOLDER>\t<RELATIVE_TONIE_NAME>

Where:
    <LANG>                is "DE" or "EN"
    <TOP_SERIES_FOLDER>   is the top-level folder inside tonies/German or tonies/English
    <RELATIVE_TONIE_NAME> is the Tonie filename (without .nfc), optionally prefixed by
                          a nested subfolder (e.g. "GEOlino/Alles ueber Vulkane")
"""

import argparse
import json
import os
import re
import sys
import unicodedata
import urllib.request
from typing import Any, Dict, List, Optional, Set, Tuple

TONIES_JSON_URL = (
    "https://raw.githubusercontent.com/toniebox-reverse-engineering/"
    "tonies-json/release/tonies.json"
)

LANGUAGE_MAPPING: Dict[str, str] = {
    "de-de": "German",
    "en-gb": "English",
    "en-us": "English",
}

LANG_CODE_SHORT: Dict[str, str] = {
    "German": "DE",
    "English": "EN",
}

MISC_SERIES_DIRECTORY = "Other"

# Directory overrides from nortakales/flipper-zero-tonies/scripts/figures_name_tool.pyw
SERIES_DIRECTORY_OVERRIDES: Dict[str, Dict[str, str]] = {
    "English": {
        "All Engines Go": "Story Time/All Engines Go",
        "Ask the StoryBots": "Ask the Storybots",
        "Brave": "Disney and Pixar",
        "Cars": "Disney and Pixar",
        "Coco": "Disney and Pixar",
        "Cocomelon": "CoComelon",
        "Daniel Tiger's Neighborhood": "Daniel Tigers Neighborhood",
        "Disney The Muppets": "Disney the Muppets",
        "Dr. Seuss": "Dr Seuss",
        "Favorite Children's Songs": "Tonies Originals",
        "Favorite Children\u2019s Songs": "Tonies Originals",
        "Finding Dory": "Disney and Pixar",
        "Gabby's Dollhouse": "DreamWorks/Gabbys Dollhouse",
        "How to Train Your Dragon": "DreamWorks",
        "How to train your Dragon": "DreamWorks",
        "Inside Out": "Disney and Pixar",
        "Kung Fu Panda": "DreamWorks",
        "Marvel's Spidey and His Amazing Friends": "Marvels Spidey and His Amazing Friends",
        "Monsters, Inc.": "Disney and Pixar",
        "Nap Time": "Tonies Originals",
        "PAW Patrol": "Paw Patrol",
        "Potty Training": "Tonies Originals",
        "Ratatouille": "Disney and Pixar",
        "Shrek": "DreamWorks",
        "Sleepy Bear": "Sleepy Time",
        "Sleepy Crocodile": "Sleepy Time",
        "Sleepy Duck": "Sleepy Time",
        "Sleepy Friends": "Sleepy Time",
        "Sleepy Jaguar": "Sleepy Time",
        "Sleepy Octopus": "Sleepy Time",
        "Sleepy Penguin": "Sleepy Time",
        "Sleepy Rabbit": "Sleepy Time",
        "Sleepy Sheep": "Sleepy Time",
        "Sleepy Toucan": "Sleepy Time",
        "Sleepy Whale": "Sleepy Time",
        "The Incredibles": "Disney and Pixar",
        "tonies\u00ae Original": "Tonies Originals",
        "Toy Story": "Disney and Pixar",
        "Trolls": "DreamWorks/Trolls",
    },
    "German": {
        "ADAC": "Clever Tonies/ADAC",
        "Affenzahn Utopia": "Affenzahn",
        "Batman": "DC",
        "Bauernhof Set": "My First Tonies",
        "BiBiBiber hat da mal 'ne Frage": "BiBiBiber hat da mal ne Frage",
        "Conni & Co": "Book Tonies/Conni",
        "Das kleine B\u00f6se Buch": "Das kleine boese Buch",
        "DC: Batwheels": "DC",
        "Der zauberhafte Wunschbuchladen": "Book Tonies/Der zauberhafte Wunschbuchladen",
        "Die (un)langweiligste Schule der Welt": "Book Tonies/Die (un)langweiligste Schule der Welt",
        "Die drei !!!": "Die drei Ausrufezeichen",
        "Die drei ???": "Die drei Fragezeichen",
        "Die drei ??? Kids": "Die drei Fragezeichen Kids",
        "Die Sendung mit dem Elefanten": "Die Sendung mit der Maus & dem Elefanten",
        "Drachenz\u00e4hmen leicht gemacht": "Dreamworks",
        "Dschungel Set": "My First Tonies",
        "Ein M\u00e4dchen namens Willow": "Book Tonies/Ein Maedchen namens Willow",
        "Elmer": "Elmar",
        "Emmi & Einschwein": "Book Tonies/Emmi & Einschwein",
        "Frau Honig": "Book Tonies/Frau Honig",
        "Gabby's Dollhouse": "Gabbys Dollhouse",
        "GEOlino": "Clever Tonies/GEOlino",
        "Gregs Tagebuch": "Greg's Tagebuch",
        "GRO\u00dfE EXPERTEN": "GROSSE EXPERTEN",
        "Gus, der klitzekleine Ritter": "Gus",
        "herrH": "Emma die Ente",
        "Jurassic World: Neue Abenteuer": "Jurassic World",
        "Kannawoniwasein": "Book Tonies/Kannawoniwasein",
        "KoboldKroniken": "Book Tonies/KoboldKroniken",
        "Kung Fu Panda": "Dreamworks",
        "Kurt, Einhorn wider Willen": "Book Tonies/Kurt, Einhorn wider Willen",
        "K\u00e4pt'n Blaub\u00e4r": "Kaeptn Blaubaer",
        "K\u00e4pt'n Sharky": "Kaptn Sharky",
        "Leo's Tag": "Leos Tag",
        "Lieblings-Literatur": "Book Tonies/Lieblings-Literatur",
        "Little People - Big Dreams": "Clever Tonies",
        "Madagascar": "Dreamworks",
        "MARVEL Spidey und seine Super-Freunde": "Marvel Spidey und seine Super-Freunde",
        "Maus": "Die Sendung mit der Maus & dem Elefanten",
        "Micky Maus & Freunde": "Disney Micky Maus & Freunde",
        "Reise Set": "My First Tonies",
        "School of Talents": "Book Tonies/School of Talents",
        "School of Talents 4": "Book Tonies/School of Talents",
        "School of Talents 5": "Book Tonies/School of Talents",
        "Sendung mit der Maus zum h\u00f6ren": "Die Sendung mit der Maus & dem Elefanten",
        "Shaun das Schaf": "Schaun das Schaf",
        "Shrek": "Dreamworks",
        "Spenden Fuchs": "Die Gluecksfuechse",
        "Spider-Man": "Marvel",
        "Trolls": "Dreamworks",
        "Weihnachtsmann & Co. KG": "Weihnachtsmann und Co.KG",
        "Wie man 13 wird": "Book Tonies/Wie man 13 wird",
        "Wieso? Weshalb? Warum? junior": "Wieso Weshalb Warum Junior",
        "Wieso? Weshalb? Warum? Profiwissen - Set": "Wieso Weshalb Warum",
        "Wundervolle Welt der Dinosaurier und der Urzeit": "Clever Tonies",
        "Zippel": "Book Tonies/Zippel, das wirklich wahre Schlossgespenst",
        "Zippel, das wirklich wahre Schlossgespenst": "Book Tonies/Zippel, das wirklich wahre Schlossgespenst",
    },
}

ALLOWED_FILENAME_CHARACTERS = re.compile(r"[^A-Za-z0-9().,!%&+ -]")
ALLOWED_DIRNAME_CHARACTERS = re.compile(r"[^A-Za-z0-9().,!%&+' -]")

TRANSLITERATIONS: Dict[str, str] = {
    "\u00e4": "ae",
    "\u00f6": "oe",
    "\u00fc": "ue",
    "\u00c4": "Ae",
    "\u00d6": "Oe",
    "\u00dc": "Ue",
    "\u00df": "ss",
    "\u2019": "'",
    "\u2018": "'",
    "\u201c": '"',
    "\u201d": '"',
    "\u2013": "-",
    "\u2014": "-",
    "\u2026": "...",
}


def transliterate(name: str) -> str:
    for character, replacement in TRANSLITERATIONS.items():
        name = name.replace(character, replacement)
    decomposed = unicodedata.normalize("NFKD", name)
    return "".join(c for c in decomposed if not unicodedata.combining(c))


def generate_valid_name(name: str, pattern: re.Pattern, max_length: int = 140) -> Optional[str]:
    if not name:
        return None
    name = transliterate(name)
    name = pattern.sub("", name)
    name = re.sub(r"\s+", " ", name).strip()
    name = name.rstrip(".").strip()
    if not name:
        return None
    if len(name) > max_length:
        name = name[:max_length].strip()
    return name


def generate_valid_filename(name: str, max_length: int = 140) -> Optional[str]:
    return generate_valid_name(name, ALLOWED_FILENAME_CHARACTERS, max_length)


def generate_valid_dirname(name: str, max_length: int = 60) -> Optional[str]:
    return generate_valid_name(name, ALLOWED_DIRNAME_CHARACTERS, max_length)


def get_series_directory(language_folder: str, series: str) -> Optional[str]:
    override = SERIES_DIRECTORY_OVERRIDES.get(language_folder, {}).get(series)
    if not override:
        return generate_valid_dirname(series)
    parts = [generate_valid_dirname(part) for part in override.split("/")]
    if not all(parts):
        return None
    return "/".join(parts)


def is_physical_tonie(entry: Dict[str, Any], include_audiothek: bool = False) -> bool:
    model = str(entry.get("model") or "").strip()
    if not model:
        return False
    if model.startswith(("box-", "pseudo:", "99-", "0000")):
        return False
    if not include_audiothek and model.startswith("2000"):
        return False
    series = (entry.get("series") or "").strip()
    if "Coming Soon" in series:
        return False
    return True


def build_catalog(data: List[Dict[str, Any]], include_audiothek: bool = False) -> List[Tuple[str, str, str]]:
    seen: Set[Tuple[str, str, str]] = set()
    entries: List[Tuple[str, str, str]] = []

    for entry in data:
        lang_raw = entry.get("language")
        if lang_raw not in LANGUAGE_MAPPING:
            continue
        if not is_physical_tonie(entry, include_audiothek=include_audiothek):
            continue

        lang_folder = LANGUAGE_MAPPING[lang_raw]
        lang_short = LANG_CODE_SHORT[lang_folder]

        episode = (entry.get("episodes") or "").strip()
        series = (entry.get("series") or "").strip() or episode
        if not series:
            continue

        series_dir = get_series_directory(lang_folder, series)
        file_name = generate_valid_filename(episode)

        if not file_name:
            file_name = generate_valid_filename(series)
            series_dir = MISC_SERIES_DIRECTORY

        if not series_dir or not file_name:
            continue

        parts = series_dir.split("/", 1)
        top_series = parts[0]
        if len(parts) > 1 and parts[1]:
            rel_tonie = f"{parts[1]}/{file_name}"
        else:
            rel_tonie = file_name

        # Ensure limits match C struct sizes (TONIE_MAX_SERIES_LEN=64, TONIE_MAX_NAME_LEN=148)
        if len(top_series) >= 63 or len(rel_tonie) >= 142:
            continue

        key = (lang_short, top_series, rel_tonie)
        if key not in seen:
            seen.add(key)
            entries.append(key)

    entries.sort(key=lambda item: (item[0], item[1].casefold(), item[2].casefold()))
    return entries


def main() -> int:
    parser = argparse.ArgumentParser(description="Update Tonie Simulator catalog from tonies.json")
    parser.add_argument(
        "-o",
        "--output",
        default=os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "assets",
            "tonies_catalog.txt",
        ),
        help="Output path for tonies_catalog.txt",
    )
    parser.add_argument(
        "--include-audiothek",
        action="store_true",
        help="Also include digital-only mytonies Audiothek episodes (model 2000*)",
    )
    args = parser.parse_args()

    print(f"Downloading {TONIES_JSON_URL} ...")
    req = urllib.request.Request(TONIES_JSON_URL, headers={"User-Agent": "TonieSimulator-CatalogGen/1.0"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        data = json.loads(resp.read().decode("utf-8"))

    entries = build_catalog(data, include_audiothek=args.include_audiothek)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "w", encoding="utf-8", newline="\n") as f:
        for lang_short, top_series, rel_tonie in entries:
            f.write(f"{lang_short}\t{top_series}\t{rel_tonie}\n")

    de_count = sum(1 for e in entries if e[0] == "DE")
    en_count = sum(1 for e in entries if e[0] == "EN")
    de_series = len({e[1] for e in entries if e[0] == "DE"})
    en_series = len({e[1] for e in entries if e[0] == "EN"})
    size_kb = os.path.getsize(args.output) / 1024.0

    print(
        f"Wrote {len(entries)} Tonies ({size_kb:.1f} KB) to {args.output}\n"
        f"  DE: {de_series} series, {de_count} Tonies\n"
        f"  EN: {en_series} series, {en_count} Tonies"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
