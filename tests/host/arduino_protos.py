"""Mimics the Arduino IDE: declares every top-level function just above the first function definition.
Usage: arduino_protos.py in.ino out.cpp"""
import re, sys
src = open(sys.argv[1]).read().split("\n")
sig = re.compile(r"^(?!(?:if|for|while|switch|else|do|return|struct|enum|class)\b)((?:static\s+|inline\s+)?(?:const\s+)?[A-Za-z_][\w:<>]*(?:\s*[\*&])?\s+\*?[A-Za-z_]\w*\s*\([^;{}]*\))\s*\{\s*$")
protos, first = [], None
for k, line in enumerate(src):
    m = sig.match(line)
    if m:
        if first is None: first = k
        p = re.sub(r"\s*=\s*[^,)]+", "", m.group(1))   # the IDE leaves defaults out when the function is already declared
        protos.append(p + ";")
out = src[:first] + ["// --- prototypes added like the Arduino IDE ---"] + protos + src[first:]
open(sys.argv[2], "w").write("\n".join(out))
print(f"{len(protos)} prototypes inserted before line {first + 1}")
