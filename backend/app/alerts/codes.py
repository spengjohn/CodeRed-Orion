"""
Alert codes the board sends, and what each one means in plain language.

Used for the spoken Twilio call and for the frontend's alert log, so both
always describe an alert the same way.
"""

ALERT_DESCRIPTIONS = {
    # Codes the board's alert detectors send (edge/src/alerts/*.hpp).
    # 102 and 103 each cover too hot and too cold; the board's detail message
    # (shown in the app) says which.
    "101": "the baby has stopped breathing",                     # ApneaDetector
    "102": "the baby's temperature is outside the safe range",   # BabyThermalAlertDetector
    "103": "the room temperature is outside the safe range",     # AmbientThermalAlertDetector
    "104": "the baby may have left the crib",                    # BedAbsenceDetector
    # Earlier codes, kept so older test alerts still read correctly.
    "1": "the baby's temperature is below room temperature",
    "2": "the baby's temperature is at room temperature",
    "3": "the baby's temperature is high",
    "4": "the baby's breathing rate is very low",
    "5": "the baby's breathing rate is very high",
}


def alert_description(emergency_type) -> str:
    """Plain-language description of an alert code (falls back to the raw code)."""
    code = str(emergency_type).strip()
    return ALERT_DESCRIPTIONS.get(code, f"an alert with code {code}")
