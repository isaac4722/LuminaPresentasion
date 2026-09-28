// src/core/src/core/ExportadorPdf.cpp — Exportador PDF del núcleo.
//
// PDF 1.4 mínimo y correcto: catálogo, páginas 960x540 pt (16:9, el
// mismo marco que la proyección), fuentes base-14 Helvetica y
// Helvetica-Bold con WinAnsiEncoding (los acentos españoles van
// directos a un byte, sin incrustar fuentes), y corrientes de texto
// sin comprimir. La tabla xref se calcula byte a byte: Acrobat y
// visores exigentes la exigen exacta.

#include "fusion/core/Exportador.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace fusion {

namespace {

// ---------------------------------------------------------------------------
// UTF-8 → CP1252 (WinAnsi). 0xA0..0xFF pasan igual; el bloque 0x80..0x9F
// de WinAnsi tiene sus propios glifos; lo no representable → '?'.
// ---------------------------------------------------------------------------

struct ParWinAnsi { unsigned short cp; unsigned char byte; };

const ParWinAnsi kWinAnsi[] = {
    {0x20AC, 0x80}, {0x201A, 0x82}, {0x0192, 0x83}, {0x201E, 0x84},
    {0x2026, 0x85}, {0x2020, 0x86}, {0x2021, 0x87}, {0x02C6, 0x88},
    {0x2030, 0x89}, {0x0160, 0x8A}, {0x2039, 0x8B}, {0x0152, 0x8C},
    {0x017D, 0x8E}, {0x2018, 0x91}, {0x2019, 0x92}, {0x201C, 0x93},
    {0x201D, 0x94}, {0x2022, 0x95}, {0x2013, 0x96}, {0x2014, 0x97},
    {0x02DC, 0x98}, {0x2122, 0x99}, {0x0161, 0x9A}, {0x203A, 0x9B},
    {0x0153, 0x9C}, {0x017E, 0x9E}, {0x0178, 0x9F},
};

// Decodifica UN punto de código UTF-8; devuelve cuántos bytes consumió
// (0 si es inválido) y escribe el punto en *cp.
int DecodificarUtf8(const std::string& s, size_t i, unsigned* cp) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { *cp = c; return 1; }
    int n = 0; unsigned v = 0;
    if ((c & 0xE0) == 0xC0) { n = 2; v = c & 0x1Fu; }
    else if ((c & 0xF0) == 0xE0) { n = 3; v = c & 0x0Fu; }
    else if ((c & 0xF8) == 0xF0) { n = 4; v = c & 0x07u; }
    else return 0;
    if (i + n > s.size()) return 0;
    for (int k = 1; k < n; ++k) {
        const unsigned char ck = static_cast<unsigned char>(s[i + k]);
        if ((ck & 0xC0) != 0x80) return 0;
        v = (v << 6) | (ck & 0x3Fu);
    }
    *cp = v;
    return n;
}

std::string Utf8aWinAnsi(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        unsigned cp = 0;
        const int n = DecodificarUtf8(s, i, &cp);
        if (n == 0) { out += '?'; ++i; continue; }
        i += static_cast<size_t>(n);
        if (cp < 0x80) { out += static_cast<char>(cp); continue; }
        if (cp >= 0xA0 && cp <= 0xFF) { out += static_cast<char>(cp); continue; }
        bool hallado = false;
        for (const auto& p : kWinAnsi) {
            if (p.cp == cp) { out += static_cast<char>(p.byte); hallado = true; break; }
        }
        if (!hallado) out += '?';
    }
    return out;
}

std::string EscaparLiteralPdf(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\' || c == '(' || c == ')') out += '\\';
        out += c;
    }
    return out;
}

std::string Num10(uint32_t v) {
    char buf[11];
    std::snprintf(buf, sizeof(buf), "%010u", static_cast<unsigned>(v));
    return buf;
}

// ---------------------------------------------------------------------------
// Generador: acumula objetos y calcula offsets.
// ---------------------------------------------------------------------------

class PdfBuilder {
public:
    PdfBuilder() {
        buf_ = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    }

    void Objeto(const std::string& cuerpo) {
        offsets_.push_back(static_cast<uint32_t>(buf_.size()));
        buf_ += std::to_string(offsets_.size()) + " 0 obj\n" + cuerpo +
                "\nendobj\n";
    }

    std::string Terminar() {
        // xref: 0 (libre) + N objetos, offsets a 10 dígitos.
        const uint32_t inicio_xref = static_cast<uint32_t>(buf_.size());
        const size_t n = offsets_.size();
        std::string xref = "xref\n0 " + std::to_string(n + 1) + "\n";
        xref += "0000000000 65535 f \n";
        for (uint32_t off : offsets_)
            xref += Num10(off) + " 00000 n \n";
        xref += "trailer\n<< /Size " + std::to_string(n + 1) +
                " /Root 1 0 R >>\nstartxref\n" + std::to_string(inicio_xref) +
                "\n%%EOF\n";
        return buf_ + xref;
    }

private:
    std::string buf_;
    std::vector<uint32_t> offsets_;
};

// Envuelve una línea a ~95 caracteres cortando en espacios.
void Envolver(const std::string& texto, std::vector<std::string>* out) {
    if (texto.empty()) return;
    const size_t kMax = 95;
    size_t i = 0;
    for (;;) {
        if (texto.size() - i <= kMax) {
            out->push_back(texto.substr(i));
            return;
        }
        size_t corte = texto.find_last_of(' ', i + kMax);
        if (corte == std::string::npos || corte <= i) corte = i + kMax;
        out->push_back(texto.substr(i, corte - i));
        i = (corte < texto.size() && texto[corte] == ' ') ? corte + 1 : corte;
    }
}

bool EscribirArchivo(const std::string& ruta, const std::string& bytes) {
    std::FILE* f = std::fopen(ruta.c_str(), "wb");
    if (!f) return false;
    const size_t n = bytes.size();
    const size_t escritos = n == 0 ? 0 : std::fwrite(bytes.data(), 1, n, f);
    std::fclose(f);
    return escritos == n;
}

} // namespace

bool Exportador::ExportarPdf(const Programa& p, const std::string& ruta_salida,
                             ResultadoExport* out) {
    if (!out) return false;
    *out = ResultadoExport{};

    const PlanExportResultado plan =
        PlanExport::Planificar(p, FormatoExportacion::Pdf);
    if (!plan.ok) {
        out->msg_error = plan.msg_error;
        return false;
    }
    out->avisos.insert(out->avisos.end(), plan.avisos.begin(),
                       plan.avisos.end());

    const std::vector<UnidadExport> unidades =
        PlanExport::EnumerarUnidades(p);

    // Numeración determinista de objetos:
    //   1 catálogo, 2 páginas, 3/4 fuentes, página i → 5+2i, contenido → 6+2i.
    std::string kids;
    int paginas_con_recorte = 0;
    std::vector<std::string> objetos_pagina;
    objetos_pagina.reserve(unidades.size() * 2);

    for (size_t i = 0; i < unidades.size(); ++i) {
        const UnidadExport& u = unidades[i];
        const size_t idx_pagina = 5 + 2 * i;
        const size_t idx_contenido = idx_pagina + 1;
        kids += std::to_string(idx_pagina) + " 0 R ";

        std::vector<std::string> lineas;
        if (!u.titulo.empty()) lineas.push_back(u.titulo);
        if (u.tipo == UnidadExport::Tipo::Texto) {
            for (const auto& l : u.lineas) {
                std::string trozo;
                for (char c : l) {
                    if (c == '\n') { lineas.push_back(trozo); trozo.clear(); }
                    else trozo.push_back(c);
                }
                lineas.push_back(trozo);
            }
        } else {
            lineas.push_back(u.ruta.empty() ? "(sin archivo)" : u.ruta);
            lineas.push_back(u.tipo == UnidadExport::Tipo::Pptx
                                 ? "(paquete pptx no incrustado, v1)"
                                 : "(medio no incrustado, v1)");
        }

        // Envolver: el título es la línea 0.
        std::vector<std::string> titulo_envuelto, cuerpo_envuelto;
        Envolver(lineas.empty() ? std::string() : lineas[0], &titulo_envuelto);
        for (size_t k = 1; k < lineas.size(); ++k)
            Envolver(lineas[k], &cuerpo_envuelto);

        std::string st;
        int y = 470;
        for (const auto& t : titulo_envuelto) {
            if (y < 60) break;
            st += "BT /F2 22 Tf 60 " + std::to_string(y) + " Td (" +
                  EscaparLiteralPdf(Utf8aWinAnsi(t)) + ") Tj ET\n";
            y -= 30;
        }
        y -= 12;
        for (const auto& l : cuerpo_envuelto) {
            if (y < 40) { ++paginas_con_recorte; break; }
            st += "BT /F1 16 Tf 60 " + std::to_string(y) + " Td (" +
                  EscaparLiteralPdf(Utf8aWinAnsi(l)) + ") Tj ET\n";
            y -= 26;
        }

        const std::string cuerpo_stream =
            "<< /Length " + std::to_string(st.size()) + " >>\nstream\n" + st +
            "endstream";
        objetos_pagina.push_back(
            "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 960 540] "
            "/Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> "
            "/Contents " + std::to_string(idx_contenido) + " 0 R >>");
        objetos_pagina.push_back(cuerpo_stream);
    }

    if (paginas_con_recorte > 0) {
        out->avisos.push_back(std::to_string(paginas_con_recorte) +
                              " página(s) con texto recortado por no caber "
                              "(alcance v1)");
    }

    PdfBuilder pdf;
    pdf.Objeto("<< /Type /Catalog /Pages 2 0 R >>");
    pdf.Objeto("<< /Type /Pages /Kids [" + kids + "] /Count " +
               std::to_string(unidades.size()) + " >>");
    pdf.Objeto("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica "
               "/Encoding /WinAnsiEncoding >>");
    pdf.Objeto("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold "
               "/Encoding /WinAnsiEncoding >>");
    for (const auto& obj : objetos_pagina) pdf.Objeto(obj);

    if (!EscribirArchivo(ruta_salida, pdf.Terminar())) {
        out->msg_error = "no se pudo escribir '" + ruta_salida + "'";
        return false;
    }

    out->archivos.push_back(ruta_salida);
    out->ok = true;
    return true;
}

} // namespace fusion
