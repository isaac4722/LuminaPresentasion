#!/usr/bin/env python3
# gen_pptx_muestra_cs.py — Emite src/managed/FusionHP.Managed.Tests/PptxMuestra.cs
# (base64 del paquete de prueba) a partir de tests/native/pptx_muestra.h,
# de modo que C++ y C# prueben contra el MISMO paquete.
import base64
import re
import sys

src = '/home/z/my-project/LuminaPresentasion/tests/native/pptx_muestra.h'
dst = '/home/z/my-project/LuminaPresentasion/src/managed/FusionHP.Managed.Tests/PptxMuestra.cs'

buf = open(src).read()
m = re.search(r'kPptx\[\] = \{(.*?)\};', buf, re.S)
data = bytes(int(x) for x in m.group(1).replace('\n', '').split(','))
b64 = base64.b64encode(data).decode('ascii')

cs = """// src/managed/FusionHP.Managed.Tests/PptxMuestra.cs
// Paquete .pptx de prueba (base64) — el MISMO paquete que usa el test
// nativo C++ (tests/native/pptx_muestra.h). Regenerado por
// scripts/gen_pptx_muestra_cs.py — NO editar a mano.

namespace FusionHP.Managed.Tests
{
    public static class PptxMuestra
    {
        public const string PptxBase64 =
            "%s";

        public const string PptxConMacrosBase64 =
            "%s";
    }
}
""" % ('",\n            "'.join(b64[i:i+76] for i in range(0, len(b64), 76)),
       base64.b64encode(b'PK\x05\x06' + b'\x00' * 18).decode('ascii'))

open(dst, 'w').write(cs)
sys.stderr.write('PptxMuestra.cs: %d bytes de paquete, base64 %d chars\n'
                 % (len(data), len(b64)))
