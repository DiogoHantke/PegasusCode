"""Referência lexical dos algoritmos locais anteriores à modularização.
Atualizar explicitamente a referência quando o autor alterar os algoritmos.
"""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
checks = json.loads((root / "tests/host/algorithm_baseline.json").read_text())
for filename, (signature, expected) in checks.items():
    source = (root / filename).read_text()
    start = source.index("{", source.index(signature))
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    body = re.sub(r"//[^\n]*|/\*.*?\*/", "", source[start:end], flags=re.S)
    tokens = re.findall(r"\w+|[^\s]", body)
    actual = hashlib.sha256(" ".join(tokens).encode()).hexdigest()
    assert actual == expected, f"Algoritmo alterado: {filename}"
print("PASS preservação lexical dos algoritmos originais")
