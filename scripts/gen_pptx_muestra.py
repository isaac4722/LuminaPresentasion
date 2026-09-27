#!/usr/bin/env python3
# gen_pptx_muestra.py — Genera un paquete .pptx mínimo (ISO/IEC-29500) de
# prueba y lo emite como cabecera C con los bytes en hexadecimal, para
# embeberlo en tests/native/pptx_muestra.h. También emite un vector de
# prueba para el inflador RFC 1951 (deflate crudo).
#
# Uso: python3 gen_pptx_muestra.py > /home/z/my-project/LuminaPresentasion/tests/native/pptx_muestra.h
import binascii
import io
import zlib

# --- vectores para pruebas unitarias del inflador -------------------------
texto_vector = "Bendecid al Señor con alegría"
co = zlib.compressobj(9, zlib.DEFLATED, -15)  # deflate crudo (RFC 1951)
deflate_crudo = co.compress(texto_vector.encode("utf-8")) + co.flush()

# --- paquete pptx mínimo ---------------------------------------------------
content_types = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Default Extension="png" ContentType="image/png"/>
<Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>
<Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/>
<Override PartName="/ppt/slides/slide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>
<Override PartName="/ppt/slides/slide2.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>
<Override PartName="/ppt/slides/slide3.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>
</Types>"""

root_rels = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="ppt/presentation.xml"/>
</Relationships>"""

pres_rels = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="slideMasters/slideMaster1.xml"/>
<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide1.xml"/>
<Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide2.xml"/>
<Relationship Id="rId4" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide3.xml"/>
</Relationships>"""

presentation = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:presentation xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
<p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst>
<p:sldIdLst><p:sldId id="256" r:id="rId2"/><p:sldId id="257" r:id="rId3"/><p:sldId id="258" r:id="rId4"/></p:sldIdLst>
<p:sldSz cx="12192000" cy="6858000"/>
<p:notesSz cx="6858000" cy="9144000"/>
</p:presentation>"""

slide_master = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sldMaster xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">
<p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/></p:spTree></p:cSld>
<p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folHlink"/>
<p:sldLayoutIdLst/></p:sldMaster>"""


def shape(titulo: bool, texto_ph: str, parrafos):
    ph = ' type="ctrTitle"' if titulo else ' type="body"'
    ps = "".join(
        "<a:p><a:r><a:t>%s</a:t></a:r></a:p>" % p for p in parrafos)
    return (
        '<p:sp><p:nvSpPr><p:cNvPr id="2" name="Forma"/>'
        '<p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr>'
        '<p:nvPr><p:ph%s idx="1"/></p:nvPr></p:nvSpPr>'
        '<p:spPr/><p:txBody><a:bodyPr/><a:lstStyle/>%s</p:txBody></p:sp>'
        % (ph, ps) if not texto_ph else
        '<p:sp><p:nvSpPr><p:cNvPr id="2" name="Forma"/>'
        '<p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr>'
        '<p:nvPr><p:ph%s/></p:nvPr></p:nvSpPr>'
        '<p:spPr/><p:txBody><a:bodyPr/><a:lstStyle/>%s</p:txBody></p:sp>'
        % (ph, ps))


slide1 = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" '
    'xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">'
    '<p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>'
    # título con entidad XML
    + shape(True, False, ["Santo es el Señor &amp; Rey"])
    + shape(False, True, ["Bendecid al Señor", "Porque es bueno",
                          "Para siempre es su misericordia"])
    + "</p:spTree></p:cSld></p:sld>")

# slide2: ctrTitle + subTitle (subTitle NO es título), runs partidos a mano,
# y fondo sólido explícito de diapositiva (para el importador: doc 9.2.6)
slide2 = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" '
    'xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">'
    '<p:cSld>'
    '<p:bg><p:bgPr><a:solidFill><a:srgbClr val="1A2B3C"/></a:solidFill>'
    '<a:effectLst/></p:bgPr></p:bg>'
    '<p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>'
    '<p:sp><p:nvSpPr><p:cNvPr id="2" name="T"/><p:cNvSpPr/><p:nvPr><p:ph type="ctrTitle"/></p:nvPr></p:nvSpPr>'
    '<p:spPr/><p:txBody><a:bodyPr/><a:lstStyle/>'
    '<a:p><a:r><a:t>Gloria</a:t></a:r><a:r><a:t> a Dios</a:t></a:r></a:p>'
    '</p:txBody></p:sp>'
    '<p:sp><p:nvSpPr><p:cNvPr id="3" name="S"/><p:cNvSpPr/><p:nvPr><p:ph type="subTitle" idx="1"/></p:nvPr></p:nvSpPr>'
    '<p:spPr/><p:txBody><a:bodyPr/><a:lstStyle/>'
    '<a:p><a:r><a:t>En las alturas</a:t></a:r></a:p>'
    '</p:txBody></p:sp>'
    "</p:spTree></p:cSld></p:sld>")

# slide3: sin título, un cuadro de texto con tamaño uniforme (sz=3200 ->
# 32 pt) y una tabla; imagen embebida p:pic -> ppt/media/image1.png
slide3 = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" '
    'xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" '
    'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'
    '<p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>'
    '<p:sp><p:nvSpPr><p:cNvPr id="2" name="Tx"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>'
    '<p:spPr/><p:txBody><a:bodyPr/><a:lstStyle/>'
    '<a:p><a:pPr/><a:r><a:rPr sz="3200"/><a:t>Texto libre</a:t></a:r></a:p>'
    '</p:txBody></p:sp>'
    '<p:pic><p:nvPicPr><p:cNvPr id="3" name="Foto"/><p:cNvPicPr/><p:nvPr/></p:nvPicPr>'
    '<p:blipFill><a:blip r:embed="rId1"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>'
    '<p:spPr/></p:pic>'
    '<p:graphicFrame><p:nvGraphicFramePr><p:cNvPr id="4" name="Tabla"/>'
    '<p:cNvGraphicFramePr/><p:nvPr/></p:nvGraphicFramePr>'
    '<a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/table">'
    '<a:tbl><a:tblPr/><a:tblGrid><a:gridCol w="100000"/></a:tblGrid>'
    '<a:tr h="100000"><a:tc><a:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>Cordero de Dios</a:t></a:r></a:p></a:txBody></a:tc></a:tr>'
    '</a:tbl></a:graphicData></a:graphic></p:graphicFrame>'
    "</p:spTree></p:cSld></p:sld>")

slide3_rels = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
    '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/image1.png"/>'
    '</Relationships>')

# PNG 1x1 rojo válido (firma y CRC correctos)
import struct, zlib as _z
def _png_1x1():
    def chunk(t, d):
        c = t + d
        return struct.pack('>I', len(d)) + c + struct.pack('>I', _z.crc32(c) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', _z.compress(b'\x00\xff\x00\x00'))
            + chunk(b'IEND', b''))
imagen1 = _png_1x1()

partes = {
    "[Content_Types].xml": content_types,
    "_rels/.rels": root_rels,
    "ppt/presentation.xml": presentation,
    "ppt/_rels/presentation.xml.rels": pres_rels,
    "ppt/slideMasters/slideMaster1.xml": slide_master,
    "ppt/slides/slide1.xml": slide1,
    "ppt/slides/slide2.xml": slide2,
    "ppt/slides/slide3.xml": slide3,
    "ppt/slides/_rels/slide3.xml.rels": slide3_rels,
    "ppt/media/image1.png": imagen1,
}

buf = io.BytesIO()
with __import__("zipfile").ZipFile(buf, "w", __import__("zipfile").ZIP_DEFLATED) as z:
    for nombre, contenido in partes.items():
        datos = contenido.encode("utf-8") if isinstance(contenido, str) else contenido
        z.writestr(nombre, datos)

pptx = buf.getvalue()

# --- emisión de la cabecera C ---------------------------------------------
print("// tests/native/pptx_muestra.h — Paquete .pptx mínimo de prueba.")
print("// Generado por scripts/gen_pptx_muestra.py — NO editar a mano.")
print("// 3 diapositivas: título+entidad, runs partidos+subTitle, tabla sin título.")
print("#pragma once")
print()
print("#include <cstddef>")
print()
print("namespace pptx_muestra {")
print()
print("inline constexpr unsigned char kDeflateCrudo[] = {")
print(",".join(str(b) for b in deflate_crudo))
print("};")
print()
print("inline constexpr size_t kDeflateCrudoTam = %d;" % len(deflate_crudo))
print("inline constexpr char kDeflateCrudoTexto[] = \"%s\";" % texto_vector)
print()
print("inline constexpr unsigned char kPptx[] = {")
print(",".join(str(b) for b in pptx))
print("};")
print()
print("inline constexpr size_t kPptxTam = %d;" % len(pptx))
print()
print("} // namespace pptx_muestra")

import sys
sys.stderr.write("pptx: %d bytes, deflate vector: %d bytes\n"
                 % (len(pptx), len(deflate_crudo)))
