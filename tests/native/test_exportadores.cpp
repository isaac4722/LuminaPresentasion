// tests/native/test_exportadores.cpp — Exportadores reales del núcleo
// (Sección 9.3): enumeración de unidades, escritor PPTX (round-trip con
// nuestro propio lector), PDF (estructura + WinAnsi) y PNG/imágenes.

#include "doctest.h"
#include "fusion/core/Exportador.h"
#include "fusion/core/PptxDirecto.h"
#include "fusion/data/AhpFormat.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace fusion;

// ---------------------------------------------------------------------------
// Ayudas
// ---------------------------------------------------------------------------

namespace {

int g_contador = 0;

// Carpeta temporal por prueba (TEMP/TMP/TMPDIR o '.'); se crea.
std::string CarpetaTmp(const char* etiqueta) {
    const char* candidatos[] = {"TMPDIR", "TEMP", "TMP"};
    std::string base = ".";
    for (const char* c : candidatos) {
        const char* v = std::getenv(c);
        if (v && *v) { base = v; break; }
    }
    std::string dir = base + "/fusion_test_export_" +
                      std::to_string(++g_contador) + "_" + etiqueta;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

Programa ProgramaDePrueba() {
    Programa p;
    p.titulo = "Culto Domingo";
    p.fecha  = "2026-09-28";
    Escenario e;
    e.id = "esc-1";
    e.nombre = "Bloque 1";
    Elemento t;
    t.id = "el-1";
    t.tipo = TipoElemento::Texto;
    t.lineas = {{"linea 1", "1"}, {"linea 2", "2"}, {"linea 3", "3"}};
    Elemento v;
    v.id = "el-2";
    v.tipo = TipoElemento::Versiculo;
    v.cita = "Salmo 24:7";
    v.texto_versiculo = "Alzad oh puertas";
    Elemento img;
    img.id = "el-3";
    img.tipo = TipoElemento::Imagen;
    img.ruta = "portada.png";
    Elemento vid;
    vid.id = "el-4";
    vid.tipo = TipoElemento::Video;
    vid.ruta = "intro.mp4";
    e.elementos = {t, v, img, vid};
    p.escenarios.push_back(e);
    return p;
}

// CRC32 local para verificar los chunks PNG (polinomio ZIP estándar).
uint32_t Crc32Test(const unsigned char* d, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c ^= d[i];
        for (int k = 0; k < 8; ++k)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return c ^ 0xFFFFFFFFu;
}

struct ChunkPng {
    std::string tipo;
    std::string datos;
};

bool PartirChunks(const std::string& png, std::vector<ChunkPng>* out) {
    if (png.size() < 8) return false;
    static const unsigned char firma[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A,
                                           0x1A, 0x0A};
    if (std::memcmp(png.data(), firma, 8) != 0) return false;
    size_t pos = 8;
    while (pos + 8 <= png.size()) {
        const uint32_t lon = (static_cast<uint32_t>(
                                  static_cast<unsigned char>(png[pos])) << 24) |
                             (static_cast<uint32_t>(
                                  static_cast<unsigned char>(png[pos + 1]))
                              << 16) |
                             (static_cast<uint32_t>(
                                  static_cast<unsigned char>(png[pos + 2]))
                              << 8) |
                             static_cast<uint32_t>(
                                 static_cast<unsigned char>(png[pos + 3]));
        ChunkPng ch;
        ch.tipo = png.substr(pos + 4, 4);
        if (pos + 8 + lon + 4 > png.size()) return false;
        ch.datos = png.substr(pos + 8, lon);
        const uint32_t crc =
            Crc32Test(reinterpret_cast<const unsigned char*>(png.data() +
                                                             pos + 4),
                      lon + 4);
        const uint32_t crc_guardado =
            (static_cast<uint32_t>(static_cast<unsigned char>(png[pos + 8 + lon])) << 24) |
            (static_cast<uint32_t>(static_cast<unsigned char>(png[pos + 9 + lon])) << 16) |
            (static_cast<uint32_t>(static_cast<unsigned char>(png[pos + 10 + lon])) << 8) |
            static_cast<uint32_t>(static_cast<unsigned char>(png[pos + 11 + lon]));
        if (crc != crc_guardado) return false;
        out->push_back(ch);
        pos += 12 + lon;
        if (ch.tipo == "IEND") break;
    }
    return !out->empty();
}

// Infla SOLO bloques almacenados (lo que emite CodificarPng) y verifica
// el adler32 del flujo zlib.
bool DescomprimirZlibAlmacenado(const std::string& idat, std::string* out) {
    if (idat.size() < 6) return false;
    if (static_cast<unsigned char>(idat[0]) != 0x78) return false;
    size_t pos = 2;
    for (;;) {
        if (pos + 5 > idat.size()) return false;
        const unsigned char cab =
            static_cast<unsigned char>(idat[pos++]);
        const bool ultimo = (cab & 1) != 0;
        if (((cab >> 1) & 3) != 0) return false;  // solo almacenados
        const size_t lon =
            static_cast<size_t>(static_cast<unsigned char>(idat[pos])) |
            (static_cast<size_t>(static_cast<unsigned char>(idat[pos + 1]))
             << 8);
        const size_t nlon =
            static_cast<size_t>(static_cast<unsigned char>(idat[pos + 2])) |
            (static_cast<size_t>(static_cast<unsigned char>(idat[pos + 3]))
             << 8);
        pos += 4;
        if ((lon ^ 0xFFFFu) != nlon) return false;
        if (pos + lon > idat.size()) return false;
        out->append(idat, pos, lon);
        pos += lon;
        if (ultimo) break;
    }
    if (pos + 4 > idat.size()) return false;
    uint32_t ad = 1, adb = 0;
    for (char c : *out) {
        ad = (ad + static_cast<unsigned char>(c)) % 65521u;
        adb = (adb + ad) % 65521u;
    }
    const uint32_t guardado =
        (static_cast<uint32_t>(static_cast<unsigned char>(idat[pos])) << 24) |
        (static_cast<uint32_t>(static_cast<unsigned char>(idat[pos + 1])) << 16) |
        (static_cast<uint32_t>(static_cast<unsigned char>(idat[pos + 2])) << 8) |
        static_cast<uint32_t>(static_cast<unsigned char>(idat[pos + 3]));
    return ((adb << 16) | ad) == guardado;
}

} // namespace

// ---------------------------------------------------------------------------
// Enumeración de unidades (fuente única del plan y de los exportadores)
// ---------------------------------------------------------------------------

TEST_CASE("Exportar: EnumerarUnidades coincide con ContarUnidades") {
    const Programa p = ProgramaDePrueba();
    const auto unidades = PlanExport::EnumerarUnidades(p);
    CHECK(unidades.size() == 6);
    CHECK(PlanExport::ContarUnidades(p) ==
          static_cast<int>(unidades.size()));

    REQUIRE(unidades.size() == 6);
    CHECK(unidades[0].tipo == UnidadExport::Tipo::Texto);
    REQUIRE(unidades[0].lineas.size() == 1);
    CHECK(unidades[0].lineas[0] == "linea 1");
    CHECK(unidades[1].lineas[0] == "linea 2");
    CHECK(unidades[2].lineas[0] == "linea 3");
    CHECK(unidades[3].tipo == UnidadExport::Tipo::Texto);
    CHECK(unidades[3].lineas[0] == "Alzad oh puertas");
    CHECK(unidades[4].tipo == UnidadExport::Tipo::Medio);
    CHECK(unidades[4].ruta == "portada.png");
    CHECK(unidades[5].tipo == UnidadExport::Tipo::Medio);
    CHECK(unidades[5].ruta == "intro.mp4");
    // Procedencia completa para el informe de exportación.
    CHECK(unidades[0].escenario == "Bloque 1");
    CHECK(unidades[0].indice_escenario == 1);
    CHECK(unidades[4].indice_elemento == 3);
}

TEST_CASE("Exportar: texto sin líneas y versículo sin líneas conservan el conteo") {
    Programa p;
    Escenario e;
    e.id = "esc-1";
    e.nombre = "Unico";
    Elemento t;
    t.id = "a";
    t.tipo = TipoElemento::Texto;
    t.titulo = "Solo titulo";
    Elemento v;
    v.id = "b";
    v.tipo = TipoElemento::Versiculo;
    v.texto_versiculo = "texto del versiculo";
    Elemento ppt;
    ppt.id = "c";
    ppt.tipo = TipoElemento::Pptx;
    ppt.ruta = "x.pptx";
    Elemento lt;
    lt.id = "d";
    lt.tipo = TipoElemento::LowerThird;
    lt.titulo = "Pie";
    e.elementos = {t, v, ppt, lt};
    p.escenarios.push_back(e);

    const auto unidades = PlanExport::EnumerarUnidades(p);
    CHECK(unidades.size() == 4);
    CHECK(PlanExport::ContarUnidades(p) == 4);
    CHECK(unidades[0].lineas[0] == "Solo titulo");
    CHECK(unidades[1].lineas[0] == "texto del versiculo");
    CHECK(unidades[2].tipo == UnidadExport::Tipo::Pptx);
    CHECK(unidades[3].tipo == UnidadExport::Tipo::Medio);
}

// ---------------------------------------------------------------------------
// Exportador PPTX: round-trip con nuestro propio lector (CRC verificado)
// ---------------------------------------------------------------------------

TEST_CASE("ExportarPptx: round-trip — el lector del núcleo relee el paquete") {
    const Programa p = ProgramaDePrueba();
    const std::string dir = CarpetaTmp("pptx_rt");
    const std::string ruta = dir + "/salida.pptx";

    ResultadoExport out;
    REQUIRE(Exportador::ExportarPptx(p, ruta, &out));
    REQUIRE(out.archivos.size() == 1);
    CHECK(out.archivos[0] == ruta);

    std::vector<DiapositivaPptx> diapos;
    const InfoPptx info = LectorPptx::LeerArchivo(ruta, &diapos);
    CHECK(info.ok);
    CHECK(info.msg_error.empty());
    CHECK(info.total_diapositivas == 6);
    REQUIRE(diapos.size() == 6);
    CHECK(info.ancho_emu == 12192000);
    CHECK(info.alto_emu == 6858000);

    // Diapositiva 1: la línea del texto, sin título en el elemento.
    REQUIRE(diapos[0].parrafos.size() == 1);
    CHECK(diapos[0].parrafos[0] == "linea 1");
    // Diapositiva 4: el versículo.
    CHECK(diapos[3].parrafos[0] == "Alzad oh puertas");
    // Diapositiva 5: referencia al medio + aviso.
    REQUIRE(diapos[4].parrafos.size() == 2);
    CHECK(diapos[4].parrafos[0] == "portada.png");
    CHECK(diapos[4].parrafos[1].find("v1") != std::string::npos);

    bool avisa_medio = false;
    for (const auto& a : out.avisos)
        if (a.find("no incrustado") != std::string::npos) avisa_medio = true;
    CHECK(avisa_medio);

    std::remove(ruta.c_str());
}

TEST_CASE("ExportarPptx: título del elemento, entidades XML y acentos") {
    Programa p;
    p.titulo = "Titulos";
    Escenario e;
    e.id = "e1";
    e.nombre = "Esc";
    Elemento t;
    t.id = "a";
    t.tipo = TipoElemento::Texto;
    t.titulo = "Cantad & al Señor";
    t.lineas = {{"\"Gloria\" <a Dios>", ""}};
    e.elementos.push_back(t);
    p.escenarios.push_back(e);

    const std::string dir = CarpetaTmp("pptx_xml");
    const std::string ruta = dir + "/ent.pptx";
    ResultadoExport out;
    REQUIRE(Exportador::ExportarPptx(p, ruta, &out));

    std::vector<DiapositivaPptx> diapos;
    const InfoPptx info = LectorPptx::LeerArchivo(ruta, &diapos);
    REQUIRE(info.ok);
    REQUIRE(diapos.size() == 1);
    REQUIRE(diapos[0].parrafos.size() == 2);
    CHECK(diapos[0].parrafos[0] == "Cantad & al Señor");
    CHECK(diapos[0].parrafos[1] == "\"Gloria\" <a Dios>");

    std::remove(ruta.c_str());
}

TEST_CASE("ExportarPptx: errores — programa vacío y ruta imposible") {
    Programa vacio;
    ResultadoExport out;
    CHECK_FALSE(Exportador::ExportarPptx(vacio, "/noexiste/a.pptx", &out));
    CHECK_FALSE(out.msg_error.empty());

    const Programa p = ProgramaDePrueba();
    ResultadoExport out2;
    CHECK_FALSE(Exportador::ExportarPptx(p, "/noexiste_dir_x/y.pptx", &out2));
    CHECK(out2.msg_error.find("no se pudo escribir") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Exportador PDF: estructura y WinAnsi
// ---------------------------------------------------------------------------

namespace {
std::string LeerBytes(const std::string& ruta) {
    std::FILE* f = std::fopen(ruta.c_str(), "rb");
    if (!f) return "";
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}
} // namespace

TEST_CASE("ExportarPdf: cabecera, xref exacta y final de archivo") {
    const Programa p = ProgramaDePrueba();
    const std::string dir = CarpetaTmp("pdf_xref");
    const std::string ruta = dir + "/salida.pdf";
    ResultadoExport out;
    REQUIRE(Exportador::ExportarPdf(p, ruta, &out));
    REQUIRE(out.archivos.size() == 1);

    const std::string pdf = LeerBytes(ruta);
    CHECK(pdf.compare(0, 8, "%PDF-1.4") == 0);
    REQUIRE(pdf.size() >= 6);
    CHECK(pdf.compare(pdf.size() - 6, 6, "%%EOF\n") == 0);
    CHECK(pdf.find("/Type /Catalog") != std::string::npos);
    CHECK(pdf.find("/Count 6") != std::string::npos);
    CHECK(pdf.find("/MediaBox [0 0 960 540]") != std::string::npos);
    CHECK(pdf.find("/WinAnsiEncoding") != std::string::npos);

    // La xref apunta byte exacto a cada "N 0 obj".
    const size_t sx = pdf.find("startxref\n");
    REQUIRE(sx != std::string::npos);
    const uint32_t inicio_xref =
        static_cast<uint32_t>(std::strtoul(pdf.c_str() + sx + 10, nullptr, 10));
    CHECK(pdf.compare(inicio_xref, 4, "xref") == 0);

    size_t pos = pdf.find("xref\n") + 5;
    // Encabezado de subsección: "0 N".
    const size_t fin_hdr = pdf.find('\n', pos);
    REQUIRE(fin_hdr != std::string::npos);
    const std::string hdr = pdf.substr(pos, fin_hdr - pos);
    CHECK(hdr == "0 17");
    pos = fin_hdr + 1;

    int objeto = 0;
    for (;;) {
        const size_t fin_linea = pdf.find('\n', pos);
        REQUIRE(fin_linea != std::string::npos);
        const std::string linea = pdf.substr(pos, fin_linea - pos);
        pos = fin_linea + 1;
        if (linea.find("trailer") != std::string::npos) break;
        if (objeto == 0) {  // entrada libre
            CHECK(linea == "0000000000 65535 f ");
            ++objeto;
            continue;
        }
        if (linea.size() < 19) break;  // entrada xref: 20 bytes con EOL
        const uint32_t off =
            static_cast<uint32_t>(std::strtoul(linea.c_str(), nullptr, 10));
        CHECK(linea[10] == ' ');
        CHECK(linea.substr(11, 6) == "00000 ");
        CHECK(linea[17] == 'n');
        CHECK(pdf.compare(off, std::to_string(objeto).size() + 6,
                          std::to_string(objeto) + " 0 obj") == 0);
        ++objeto;
    }
    CHECK(objeto == 17);  // entrada libre + 4 fijos + 6 páginas x 2 objetos

    std::remove(ruta.c_str());
}

TEST_CASE("ExportarPdf: acentos WinAnsi y escapes de literales") {
    Programa p;
    p.titulo = "Acentos";
    Escenario e;
    e.id = "e1";
    e.nombre = "Esc";
    Elemento t;
    t.id = "a";
    t.tipo = TipoElemento::Texto;
    t.lineas = {{"Cantad al Señor ¡aleluya! (todos)", ""}};
    e.elementos.push_back(t);
    p.escenarios.push_back(e);

    const std::string dir = CarpetaTmp("pdf_wa");
    const std::string ruta = dir + "/acentos.pdf";
    ResultadoExport out;
    REQUIRE(Exportador::ExportarPdf(p, ruta, &out));

    const std::string pdf = LeerBytes(ruta);
    // 'ñ' en WinAnsi es 0xF1 y '¡' es 0xA1.
    CHECK(pdf.find("Se\xF1or") != std::string::npos);
    CHECK(pdf.find("\xA1""aleluya!") != std::string::npos);
    // Paréntesis escapados en la literal.
    CHECK(pdf.find("\\(todos\\)") != std::string::npos);

    std::remove(ruta.c_str());
}

TEST_CASE("ExportarPdf: programa vacío falla como el plan") {
    Programa vacio;
    ResultadoExport out;
    CHECK_FALSE(Exportador::ExportarPdf(vacio, "/noexiste/a.pdf", &out));
    CHECK(out.msg_error == "el programa no tiene escenarios que exportar");
}

// ---------------------------------------------------------------------------
// Codificador PNG
// ---------------------------------------------------------------------------

TEST_CASE("CodificarPng: firma, IHDR, CRC por chunk y píxeles intactos") {
    const int w = 4, h = 3;
    std::vector<unsigned char> rgba(w * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            rgba[(y * w + x) * 4 + 0] = static_cast<unsigned char>(x * 60);
            rgba[(y * w + x) * 4 + 1] = static_cast<unsigned char>(y * 80);
            rgba[(y * w + x) * 4 + 2] = static_cast<unsigned char>(x + y);
            rgba[(y * w + x) * 4 + 3] = 255;
        }

    const std::string png = Exportador::CodificarPng(w, h, rgba.data());
    REQUIRE_FALSE(png.empty());

    std::vector<ChunkPng> chunks;
    REQUIRE(PartirChunks(png, &chunks));
    REQUIRE(chunks.size() == 3);
    CHECK(chunks[0].tipo == "IHDR");
    CHECK(chunks[1].tipo == "IDAT");
    CHECK(chunks[2].tipo == "IEND");

    // IHDR: 4+4 bytes de tamaño, 8 de profundidad, 6 RGBA.
    REQUIRE(chunks[0].datos.size() == 13);
    const uint32_t w_png = (static_cast<uint32_t>(static_cast<unsigned char>(chunks[0].datos[0])) << 24) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(chunks[0].datos[1])) << 16) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(chunks[0].datos[2])) << 8) |
                           static_cast<uint32_t>(static_cast<unsigned char>(chunks[0].datos[3]));
    const uint32_t h_png = (static_cast<uint32_t>(static_cast<unsigned char>(chunks[0].datos[4])) << 24) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(chunks[0].datos[5])) << 16) |
                           (static_cast<uint32_t>(static_cast<unsigned char>(chunks[0].datos[6])) << 8) |
                           static_cast<uint32_t>(static_cast<unsigned char>(chunks[0].datos[7]));
    CHECK(w_png == 4);
    CHECK(h_png == 3);
    CHECK(static_cast<unsigned char>(chunks[0].datos[8]) == 8);
    CHECK(static_cast<unsigned char>(chunks[0].datos[9]) == 6);

    // IDAT: zlib almacenado → scanlines → píxeles exactos.
    std::string crudo;
    REQUIRE(DescomprimirZlibAlmacenado(chunks[1].datos, &crudo));
    const size_t fila = static_cast<size_t>(w) * 4 + 1;
    REQUIRE(crudo.size() == fila * h);
    for (int y = 0; y < h; ++y) {
        CHECK(static_cast<unsigned char>(crudo[y * fila]) == 0);  // filtro
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < 4; ++c) {
                CHECK(static_cast<unsigned char>(crudo[y * fila + 1 + x * 4 + c]) ==
                      rgba[(y * w + x) * 4 + c]);
            }
        }
    }
}

TEST_CASE("CodificarPng: parámetros inválidos devuelven vacío") {
    std::vector<unsigned char> rgba(16 * 4, 200);
    CHECK(Exportador::CodificarPng(0, 4, rgba.data()).empty());
    CHECK(Exportador::CodificarPng(-3, 4, rgba.data()).empty());
    CHECK(Exportador::CodificarPng(4, 4, nullptr).empty());
}

// ---------------------------------------------------------------------------
// Exportador de imágenes: el shell aporta los píxeles
// ---------------------------------------------------------------------------

TEST_CASE("ExportarImagenes: un PNG por unidad con nombre y orden estables") {
    const Programa p = ProgramaDePrueba();
    const std::string dir = CarpetaTmp("img_ok");

    RasterizadorUnidad raster =
        [](int indice, int ancho_obj, int alto_obj, int* ancho, int* alto,
           std::vector<unsigned char>* rgba) {
            (void)indice;
            CHECK(ancho_obj == 1920);
            CHECK(alto_obj == 1080);
            *ancho = 320;
            *alto = 180;
            rgba->assign(static_cast<size_t>(320) * 180 * 4, 0);
            (*rgba)[0] = static_cast<unsigned char>(indice);
            (*rgba)[3] = 255;
            return true;
        };

    ResultadoExport out;
    REQUIRE(Exportador::ExportarImagenes(p, dir, raster, &out));
    REQUIRE(out.archivos.size() == 6);
    CHECK(out.archivos[0].find("-01.png") != std::string::npos);
    CHECK(out.archivos[5].find("-06.png") != std::string::npos);
    CHECK(out.archivos[0].find(dir) == 0);

    for (const auto& ruta : out.archivos) {
        const std::string bytes = LeerBytes(ruta);
        CHECK(bytes.compare(0, 4, "\x89PNG") == 0);
        std::remove(ruta.c_str());
    }
    bool avisa = false;
    for (const auto& a : out.avisos)
        if (a.find("referencia textual") != std::string::npos) avisa = true;
    CHECK(avisa);
}

TEST_CASE("ExportarImagenes: fallo del rasterizador y buffer incoherente") {
    const Programa p = ProgramaDePrueba();

    RasterizadorUnidad falla =
        [](int indice, int, int, int* ancho, int* alto,
           std::vector<unsigned char>* rgba) {
            *ancho = 8;
            *alto = 8;
            rgba->assign(8 * 8 * 4, 0);
            return indice != 2;  // la unidad 2 falla
        };
    ResultadoExport out;
    CHECK_FALSE(Exportador::ExportarImagenes(p, ".", falla, &out));
    CHECK(out.msg_error.find("unidad 2") != std::string::npos);
    // La unidad 1 sí se había escrito: el informe lo declara.
    REQUIRE(out.archivos.size() == 1);
    CHECK(out.archivos[0].find("-01.png") != std::string::npos);
    std::remove(out.archivos[0].c_str());

    RasterizadorUnidad corto =
        [](int, int, int, int* ancho, int* alto,
           std::vector<unsigned char>* rgba) {
            *ancho = 10;
            *alto = 10;
            rgba->assign(10 * 10 * 3, 255);  // tamaño incorrecto
            return true;
        };
    ResultadoExport out2;
    CHECK_FALSE(Exportador::ExportarImagenes(p, ".", corto, &out2));
    CHECK(out2.msg_error.find("incoherente") != std::string::npos);
}

TEST_CASE("ExportarImagenes: sin rasterizador y programa vacío") {
    const Programa p = ProgramaDePrueba();
    Programa vacio;

    ResultadoExport out;
    CHECK_FALSE(Exportador::ExportarImagenes(p, ".", RasterizadorUnidad{}, &out));
    CHECK(out.msg_error.find("rasterizador") != std::string::npos);

    RasterizadorUnidad raster = [](int, int, int, int* ancho, int* alto,
                                   std::vector<unsigned char>* rgba) {
        *ancho = 8;
        *alto = 8;
        rgba->assign(8 * 8 * 4, 0);
        return true;
    };
    ResultadoExport out2;
    CHECK_FALSE(Exportador::ExportarImagenes(vacio, ".", raster, &out2));
    CHECK(out2.msg_error == "el programa no tiene escenarios que exportar");
}
