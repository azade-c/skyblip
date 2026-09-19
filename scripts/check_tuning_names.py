#!/usr/bin/env python3
"""Fail if a tuning constant in the firmware's logic layers is named off-vocabulary.

Three rules, written out in firmware/README.md under "Naming a tuning constant":

  unit last     a name that carries a mechanism ends in the unit it is measured in
  subject first a mechanism on its own is ambiguous at the call site: kForgetMs
                forgets what? The subject comes before it
  one word each a mechanism has one approved name, so a grep for every hold in
                the tree finds every hold

`hardware/`, `boards/` and `ports/` are out of scope: those figures are a
datasheet's, under the datasheet's name, and a rename would hide where they came
from.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tuning_constants import DIMENSIONED, MECHANISMS, UNITS, collect  # noqa: E402

SYNONYMS = {
    "Delay": "Settle for waiting after an event, Floor for spacing two outputs",
    "Grace": "Hold",
    "Debounce": "Samples",
    "Steady": "Hold",
    "Consecutive": "Samples, with the subject in front of it",
}

EXEMPT = {
    "kSecondMs": "the millisecond in a second, a unit conversion and not a tuning",
    "kSecondUs": "the microsecond in a second, a unit conversion and not a tuning",
    "kHalfSecondUs": "half of the above",
    "kNominalSecondUs": "the second the PPS is measured against",
}


def failures(constants):
    for constant in constants:
        if constant.name in EXEMPT:
            continue
        if mechanism_slot(constant) in SYNONYMS:
            word = mechanism_slot(constant)
            yield constant, f"{word} is spelled {SYNONYMS[word]}"
        if any(word in DIMENSIONED for word in constant.words) and not constant.unit:
            yield constant, f"carries a mechanism and no unit, one of {units()}"
        if constant.indexed and not constant.class_scope and not subject_of(constant):
            yield constant, f"{constant.mechanism or constant.unit} of what? Name the subject"


def mechanism_slot(constant):
    return constant.subject_words[-1] if constant.subject_words else ""


def subject_of(constant):
    return [word for word in constant.subject_words if word not in MECHANISMS]


def units():
    return ", ".join(sorted(UNITS))


def main():
    constants = collect()
    broken = list(failures(constants))
    for constant, why in broken:
        report(f"{constant.where}: {constant.name}: {why}")
    if broken:
        return 1
    report(f"OK: {len(constants)} constants in the logic layers read as the vocabulary asks.")
    return 0


def report(message):
    print(message, file=sys.stderr)


if __name__ == "__main__":
    sys.exit(main())
