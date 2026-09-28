// tests/native/test_pptx_directo.cpp — Tests del lector directo de
// paquetes .pptx/.pptm (Sección 9.2, modo "directo"): ZIP propio,
// inflador RFC 1951 (verificado con el paquete embebido deflate real),
// entidades XML, orden de p:sldIdLst y tolerancia a paquetes rotos.

#include "doctest.h"
#include "fusion/core/PptxDirecto.h"
#include "fusion/core/PlanPptx.h"
#include "fusion/data/AhpFormat.h"
#include "pptx_muestra.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace fusion;

namespace {

// CRC32 de referencia (RFC 1952); su corrección se fija con el vector
// conocido "123456789" -> 0xCBF43926.
uint32_t Crc32Ref(const unsigned char* d, size_t n) {
    static uint32_t tabla[256];
    static bool lista = false;
    if (!lista) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            tabla[i] = c;
        }
        lista = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i)
        c = tabla[(c ^ d[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void Pon16(std::string& s, uint16_t v) {
    s += static_cast<char>(v & 0xFF);
    s += static_cast<char>((v >> 8) & 0xFF);
}

void Pon32(std::string& s, uint32_t v) {
    Pon16(s, static_cast<uint16_t>(v & 0xFFFF));
    Pon16(s, static_cast<uint16_t>(v >> 16));
}

// Constructor de ZIP de prueba con entradas almacenadas (método 0) y
// comentario opcional. `omitir` declara la parte en el paquete pero no
// mete sus datos (para probar diapositivas ausentes).
struct ParteZip {
    std::string nombre;
    std::string contenido;
    bool omitir = false;
};

std::string ZipDePrueba(const std::vector<ParteZip>& partes,
                        const std::string& comentario = "") {
    struct Central {
        std::string nombre;
        uint32_t crc, tam, off;
    };
    std::vector<Central> centrales;
    std::string out;
    for (const auto& p : partes) {
        if (p.omitir) continue;
        const uint32_t crc = Crc32Ref(
            reinterpret_cast<const unsigned char*>(p.contenido.data()),
            p.contenido.size());
        const uint32_t tam = static_cast<uint32_t>(p.contenido.size());
        const uint32_t off = static_cast<uint32_t>(out.size());
        Pon32(out, 0x04034b50u);          // firma de cabecera local
        Pon16(out, 20);                   // versión necesaria
        Pon16(out, 0x0800);               // banderas (UTF-8)
        Pon16(out, 0);                    // método: almacenado
        Pon16(out, 0); Pon16(out, 0);     // hora, fecha
        Pon32(out, crc);
        Pon32(out, tam);
        Pon32(out, tam);
        Pon16(out, static_cast<uint16_t>(p.nombre.size()));
        Pon16(out, 0);                    // extra local
        out += p.nombre;
        out += p.contenido;
        centrales.push_back({p.nombre, crc, tam, off});
    }
    const uint32_t cd_off = static_cast<uint32_t>(out.size());
    for (const auto& c : centrales) {
        Pon32(out, 0x02014b50u);          // firma del directorio central
        Pon16(out, 20); Pon16(out, 20);   // versiones
        Pon16(out, 0x0800);               // banderas
        Pon16(out, 0);                    // método: almacenado
        Pon16(out, 0); Pon16(out, 0);     // hora, fecha
        Pon32(out, c.crc);
        Pon32(out, c.tam);
        Pon32(out, c.tam);
        Pon16(out, static_cast<uint16_t>(c.nombre.size()));
        Pon16(out, 0);                    // extra
        Pon16(out, 0);                    // comentario
        Pon16(out, 0);                    // disco
        Pon16(out, 0);                    // atributos internos
        Pon32(out, 0);                    // atributos externos
        Pon32(out, c.off);
        out += c.nombre;
    }
    const uint32_t cd_tam = static_cast<uint32_t>(out.size()) - cd_off;
    Pon32(out, 0x06054b50u);              // EOCD
    Pon16(out, 0); Pon16(out, 0);         // nº de disco
    Pon16(out, static_cast<uint16_t>(centrales.size()));
    Pon16(out, static_cast<uint16_t>(centrales.size()));
    Pon32(out, cd_tam);
    Pon32(out, cd_off);
    Pon16(out, static_cast<uint16_t>(comentario.size()));
    out += comentario;
    return out;
}

std::string RelacionOfficeDocument(const std::string& destino) {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
           "<Relationships xmlns=\"http://schemas.openxmlformats.org/"
           "package/2006/relationships\">"
           "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats."
           "org/officeDocument/2006/relationships/officeDocument\" Target=\"" +
           destino + "\"/></Relationships>";
}

} // namespace

// ---------------------------------------------------------------------------
// Piezas sueltas
// ---------------------------------------------------------------------------

TEST_CASE("CRC32: vector conocido del RFC") {
    const char* v = "123456789";
    CHECK(Crc32Ref(reinterpret_cast<const unsigned char*>(v), 9) ==
          0xCBF43926u);
}

TEST_CASE("EsPaquetePptx: extensiones") {
    CHECK(LectorPptx::EsPaquetePptx("medios/culto.pptx"));
    CHECK(LectorPptx::EsPaquetePptx("medios/culto.PPTX"));
    CHECK(LectorPptx::EsPaquetePptx("C:\\medios\\con macros.pptm"));
    CHECK_FALSE(LectorPptx::EsPaquetePptx("medios/viejo.ppt"));
    CHECK_FALSE(LectorPptx::EsPaquetePptx("medios/cancion.zip"));
    CHECK_FALSE(LectorPptx::EsPaquetePptx("sinextension"));
    CHECK_FALSE(LectorPptx::EsPaquetePptx(".pptx"));
}

TEST_CASE("Coherencia entre el planificador y el lector de paquetes") {
    // Las mismas rutas pasan por PlanPptx (decisión del operador) y por
    // LectorPptx::EsPaquetePptx (capacidad de lectura): deben coincidir.
    const char* rutas[] = {"medios/a.pptx", "medios/A.PPTM",
                           "medios/viejo.ppt", "medios/video.mp4",
                           "sinextension", "medios/.pptx"};
    for (const char* ruta : rutas) {
        Elemento e;
        e.id = "el-pptx";
        e.tipo = TipoElemento::Pptx;
        e.ruta = ruta;
        e.modo_pptx = ModoPptx::Directo;
        const auto plan = PlanPptx::Planificar(e, false);
        CHECK(plan.ok == LectorPptx::EsPaquetePptx(ruta));
    }
}

// ---------------------------------------------------------------------------
// Paquete embebido: deflate real (zlib) — es la prueba del inflador.
// ---------------------------------------------------------------------------

TEST_CASE("Paquete embebido: cabecera del paquete") {
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        pptx_muestra::kPptx, pptx_muestra::kPptxTam, nullptr);
    REQUIRE(info.ok);
    CHECK(info.msg_error.empty());
    CHECK(info.total_diapositivas == 3);
    CHECK(info.ancho_emu == 12192000);
    CHECK(info.alto_emu == 6858000);
    CHECK(info.avisos.empty());
}

TEST_CASE("Paquete embebido: contenido exacto por diapositiva") {
    std::vector<DiapositivaPptx> diapos;
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        pptx_muestra::kPptx, pptx_muestra::kPptxTam, &diapos);
    REQUIRE(info.ok);
    REQUIRE(diapos.size() == 3);

    // Diapositiva 1: título con entidad decodificada + cuerpo en orden.
    CHECK(diapos[0].indice == 1);
    CHECK(diapos[0].titulo == "Santo es el Señor & Rey");
    REQUIRE(diapos[0].parrafos.size() == 3);
    CHECK(diapos[0].parrafos[0] == "Bendecid al Señor");
    CHECK(diapos[0].parrafos[1] == "Porque es bueno");
    CHECK(diapos[0].parrafos[2] == "Para siempre es su misericordia");

    // Diapositiva 2: runs partidos se unen; subTitle NO es título.
    CHECK(diapos[1].indice == 2);
    CHECK(diapos[1].titulo == "Gloria a Dios");
    REQUIRE(diapos[1].parrafos.size() == 1);
    CHECK(diapos[1].parrafos[0] == "En las alturas");

    // Diapositiva 3: sin título; cuadro de texto + celda de tabla.
    CHECK(diapos[2].indice == 3);
    CHECK(diapos[2].titulo.empty());
    REQUIRE(diapos[2].parrafos.size() == 2);
    CHECK(diapos[2].parrafos[0] == "Texto libre");
    CHECK(diapos[2].parrafos[1] == "Cordero de Dios");
}

TEST_CASE("Paquete embebido: salida nula también funciona") {
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        pptx_muestra::kPptx, pptx_muestra::kPptxTam, nullptr);
    REQUIRE(info.ok);
    CHECK(info.total_diapositivas == 3);
}

// ---------------------------------------------------------------------------
// Entradas almacenadas, comentario EOCD y orden de diapositivas
// ---------------------------------------------------------------------------

TEST_CASE("Entradas almacenadas y EOCD con comentario") {
    const std::string pres =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<p:presentation xmlns:p=\"http://schemas.openxmlformats.org/"
        "presentationml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/"
        "relationships\">"
        "<p:sldSz cx=\"9144000\" cy=\"6858000\"/></p:presentation>";
    const std::string zip = ZipDePrueba(
        {{"_rels/.rels", RelacionOfficeDocument("ppt/presentation.xml")},
         {"ppt/presentation.xml", pres}},
        "comentario de prueba");
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(zip.data()), zip.size(),
        nullptr);
    REQUIRE(info.ok);
    CHECK(info.ancho_emu == 9144000);
    CHECK(info.alto_emu == 6858000);
    REQUIRE(info.avisos.size() == 1);  // sin diapositivas
    CHECK(info.avisos[0].find("no tiene diapositivas") != std::string::npos);
}

TEST_CASE("Orden de diapositivas y título/placeholder") {
    const std::string rels_pres =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/"
        "package/2006/relationships\">"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/"
        "officeDocument/2006/relationships/slide\" Target=\"slides/a.xml\"/>"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/"
        "officeDocument/2006/relationships/slide\" Target=\"slides/b.xml\"/>"
        "</Relationships>";
    const std::string pres =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<p:presentation xmlns:p=\"http://schemas.openxmlformats.org/"
        "presentationml/2006/main\" xmlns:r=\"http://schemas.openxmlformats."
        "org/officeDocument/2006/relationships\">"
        "<p:sldIdLst><p:sldId id=\"256\" r:id=\"rId2\"/>"
        "<p:sldId id=\"257\" r:id=\"rId3\"/></p:sldIdLst>"
        "<p:sldSz cx=\"12192000\" cy=\"6858000\"/></p:presentation>";
    const std::string slide_b =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<p:sld xmlns:p=\"http://schemas.openxmlformats.org/"
        "presentationml/2006/main\" "
        "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">"
        "<p:cSld><p:spTree><p:sp><p:nvSpPr><p:nvPr><p:ph type=\"title\"/>"
        "</p:nvPr></p:nvSpPr><p:txBody><a:p><a:r><a:t>Segunda</a:t></a:r>"
        "</a:p></p:txBody></p:sp></p:spTree></p:cSld></p:sld>";
    // a.xml declarada pero OMITIDA: b.xml conserva su posición (2).
    const std::string zip = ZipDePrueba(
        {{"_rels/.rels", RelacionOfficeDocument("ppt/presentation.xml")},
         {"ppt/_rels/presentation.xml.rels", rels_pres},
         {"ppt/presentation.xml", pres},
         {"ppt/slides/a.xml", "", true /* omitir */},
         {"ppt/slides/b.xml", slide_b}});
    std::vector<DiapositivaPptx> diapos;
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(zip.data()), zip.size(),
        &diapos);
    REQUIRE(info.ok);
    CHECK(info.total_diapositivas == 1);
    REQUIRE(diapos.size() == 1);
    CHECK(diapos[0].indice == 2);
    CHECK(diapos[0].titulo == "Segunda");
    REQUIRE(info.avisos.size() == 1);
    CHECK(info.avisos[0].find("diapositiva 1 omitida") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Paquetes rotos: errores explícitos, sin retroceso silencioso
// ---------------------------------------------------------------------------

TEST_CASE("No es un paquete zip") {
    const char* basura = "esto no es un zip en absoluto";
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(basura),
        std::string(basura).size(), nullptr);
    CHECK_FALSE(info.ok);
    CHECK(info.msg_error.find("zip") != std::string::npos);
}

TEST_CASE("Paquete truncado: sin EOCD") {
    const std::string zip =
        ZipDePrueba({{"_rels/.rels", "<?xml version=\"1.0\"?><Relationships/>"}});
    REQUIRE(zip.size() > 10);
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(zip.data()), zip.size() - 10,
        nullptr);
    CHECK_FALSE(info.ok);
    CHECK(info.msg_error.find("zip") != std::string::npos);
}

TEST_CASE("CRC corrupto en entrada almacenada") {
    const std::string rels = RelacionOfficeDocument("ppt/presentation.xml");
    const std::string pres = "<p:presentation "
        "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"/>";
    std::string zip = ZipDePrueba(
        {{"_rels/.rels", rels}, {"ppt/presentation.xml", pres}});
    // Contenido de la 2.ª entrada: cab local 2 (30 + nombre 20) tras el
    // final de la 1.ª (30 + nombre 11 + contenido). Corromper ahí.
    const std::string n1 = "_rels/.rels";
    const std::string n2 = "ppt/presentation.xml";
    const size_t dato = 30 + n1.size() + rels.size() + 30 + n2.size() + 5;
    REQUIRE(dato < zip.size());
    zip[dato] = static_cast<char>(zip[dato] ^ 0x5A);
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(zip.data()), zip.size(),
        nullptr);
    CHECK_FALSE(info.ok);
    CHECK(info.msg_error.find("crc32") != std::string::npos);
}

TEST_CASE("Sin relación officeDocument") {
    const std::string zip = ZipDePrueba(
        {{"_rels/.rels", "<?xml version=\"1.0\"?><Relationships "
                         "xmlns=\"http://schemas.openxmlformats.org/package/"
                         "2006/relationships\"/>"}});
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(zip.data()), zip.size(),
        nullptr);
    CHECK_FALSE(info.ok);
    CHECK(info.msg_error.find("officeDocument") != std::string::npos);
}

TEST_CASE("Parte de presentación ausente") {
    const std::string zip = ZipDePrueba(
        {{"_rels/.rels", RelacionOfficeDocument("ppt/presentation.xml")}});
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(zip.data()), zip.size(),
        nullptr);
    CHECK_FALSE(info.ok);
    CHECK(info.msg_error.find("ppt/presentation.xml") != std::string::npos);
}

// ---------------------------------------------------------------------------
// v2: runs con estilo (a:rPr) e imágenes internas (p:pic + media)
// ---------------------------------------------------------------------------

namespace {

// PNG 2x2 RGBA (rojo/verde arriba, azul/amarillo abajo), 77 bytes.
const unsigned char kPng2x2[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xB6, 0x0D, 0x24, 0x00, 0x00, 0x00,
    0x14, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
    0x1F, 0x0C, 0x81, 0x34, 0x10, 0x30, 0xFC, 0x07, 0x00, 0x47, 0xCA, 0x08,
    0xF8, 0x8B, 0x4E, 0x43, 0x85, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
    0x44, 0xAE, 0x42, 0x60, 0x82};

std::string PackageConDiapositiva(const std::string& xml_slide,
                                  const std::string& rels_slide,
                                  const std::vector<std::pair<std::string, std::string>>& extra = {}) {
    std::vector<ParteZip> partes = {
        {"[Content_Types].xml",
         "<?xml version=\"1.0\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
         "<Default Extension=\"rels\" ContentType=\"a\"/><Default Extension=\"xml\" ContentType=\"b\"/>"
         "<Default Extension=\"png\" ContentType=\"image/png\"/>"
         "<Override PartName=\"/ppt/presentation.xml\" ContentType=\"c\"/></Types>"},
        {"_rels/.rels",
         "<?xml version=\"1.0\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
         "<Relationship Id=\"rDoc\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
         "Target=\"ppt/presentation.xml\"/></Relationships>"},
        {"ppt/presentation.xml",
         "<?xml version=\"1.0\"?><p:presentation xmlns:p=\"x\" xmlns:r=\"y\">"
         "<p:sldSz cx=\"9144000\" cy=\"6858000\"/>"
         "<p:sldIdLst><p:sldId id=\"256\" r:id=\"rS1\"/></p:sldIdLst></p:presentation>"},
        {"ppt/_rels/presentation.xml.rels",
         "<?xml version=\"1.0\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
         "<Relationship Id=\"rS1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide\" "
         "Target=\"slides/slide1.xml\"/></Relationships>"},
        {"ppt/slides/slide1.xml", xml_slide},
        {"ppt/slides/_rels/slide1.xml.rels", rels_slide},
    };
    for (const auto& e : extra) partes.push_back({e.first, e.second});
    return ZipDePrueba(partes);
}

std::string SlideTextoConEstilos() {
    return "<?xml version=\"1.0\"?><p:sld xmlns:p=\"x\" xmlns:a=\"z\">"
           "<p:cSld><p:spTree><p:sp>"
           "<p:nvSpPr><p:cNvPr id=\"2\" name=\"T\u00edtulo 1\"/>"
           "<p:cNvSpPr/><p:nvPr><p:ph type=\"title\"/></p:nvPr></p:nvSpPr>"
           "<p:txBody><a:p><a:r><a:rPr sz=\"4400\" b=\"1\"/><a:t>Bendecid</a:t></a:r></a:p></p:txBody>"
           "</p:sp><p:sp>"
           "<p:nvSpPr><p:cNvPr id=\"3\" name=\"Cuerpo\"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>"
           "<p:txBody>"
           "<a:p><a:r><a:t>al Se\u00f1or </a:t></a:r>"
           "<a:r><a:rPr sz=\"4000\" i=\"1\" u=\"sng\"/><a:t>con alegr\u00eda</a:t></a:r>"
           "<a:r><a:rPr sz=\"4000\" i=\"1\"><a:solidFill><a:srgbClr val=\"FFCC00\"/></a:solidFill></a:rPr>"
           "<a:t> siempre</a:t></a:r></a:p>"
           "</p:txBody></p:sp></p:spTree></p:cSld></p:sld>";
}

} // namespace

TEST_CASE("v2: runs con estilo — tamaño, negrita, cursiva, subrayado y color") {
    const std::string paquete = PackageConDiapositiva(SlideTextoConEstilos(), "<Relationships/>");
    std::vector<DiapositivaPptx> diapos;
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(paquete.data()), paquete.size(),
        &diapos);
    REQUIRE(info.ok == true);
    REQUIRE(diapos.size() == 1);
    const DiapositivaPptx& d = diapos[0];
    CHECK(d.titulo == "Bendecid");
    REQUIRE(d.parrafos_ricos.size() == 1);
    const ParrafoPptx& par = d.parrafos_ricos[0];
    REQUIRE(par.runs.size() == 3);
    // Texto plano unido sin separador (compat v1 intacta).
    REQUIRE(d.parrafos.size() == 1);
    CHECK(d.parrafos[0] == "al Se\xc3\xb1or con alegr\xc3\xad""a siempre");

    // Run 1: sin estilo explícito más allá del texto.
    CHECK(par.runs[0].texto_utf8 == "al Se\xc3\xb1or ");
    CHECK(par.runs[0].tiene_tamano == false);
    // Run 2: sz=4000 → 40 pt, cursiva, subrayado.
    CHECK(par.runs[1].texto_utf8 == "con alegr\xc3\xad" "a");
    CHECK(par.runs[1].tiene_tamano == true);
    CHECK(par.runs[1].tam_pt == doctest::Approx(40.0));
    CHECK(par.runs[1].cursiva == true);
    CHECK(par.runs[1].subrayado == true);
    CHECK(par.runs[1].negrita == false);
    // Run 3: color explícito.
    CHECK(par.runs[2].tiene_color == true);
    CHECK(par.runs[2].color_hex == "#FFCC00");
}

TEST_CASE("v2: run del título con sz/negrita legible para el render") {
    const std::string paquete = PackageConDiapositiva(SlideTextoConEstilos(), "<Relationships/>");
    std::vector<DiapositivaPptx> diapos;
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(paquete.data()), paquete.size(),
        &diapos);
    REQUIRE(info.ok == true);
    // El título conserva el texto unido de sus runs.
    CHECK(diapos[0].titulo == "Bendecid");
}

TEST_CASE("v2: imagen interna decodificada con stb_image (posición EMU)") {
    const std::string xml =
        "<?xml version=\"1.0\"?><p:sld xmlns:p=\"x\" xmlns:a=\"z\" xmlns:r=\"y\">"
        "<p:cSld><p:spTree>"
        "<p:pic><p:nvPicPr><p:cNvPr id=\"5\" name=\"Foto\"/><p:cNvPicPr/><p:nvPr/></p:nvPicPr>"
        "<p:blipFill><a:blip r:embed=\"rImg1\"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>"
        "<p:spPr><a:xfrm><a:off x=\"914400\" y=\"457200\"/><a:ext cx=\"2743200\" cy=\"1828800\"/></a:xfrm>"
        "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></p:spPr></p:pic>"
        "</p:spTree></p:cSld></p:sld>";
    const std::string rels =
        "<?xml version=\"1.0\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rImg1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" "
        "Target=\"../media/image1.png\"/></Relationships>";
    const std::string png(reinterpret_cast<const char*>(kPng2x2),
                          sizeof(kPng2x2));
    const std::string paquete = PackageConDiapositiva(xml, rels,
                                                      {{"ppt/media/image1.png", png}});
    std::vector<DiapositivaPptx> diapos;
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(paquete.data()), paquete.size(),
        &diapos);
    REQUIRE(info.ok == true);
    CHECK(info.avisos.empty());
    REQUIRE(diapos.size() == 1);
    REQUIRE(diapos[0].imagenes.size() == 1);
    const ImagenPptx& img = diapos[0].imagenes[0];
    CHECK(img.ancho == 2);
    CHECK(img.alto == 2);
    REQUIRE(img.rgba.size() == 2 * 2 * 4);
    CHECK(img.rgba[0] == 255);  // rojo en (0,0)
    CHECK(img.rgba[1] == 0);
    CHECK(img.rgba[2] == 0);
    CHECK(img.rgba[3] == 255);
    CHECK(img.x_emu == 914400);
    CHECK(img.y_emu == 457200);
    CHECK(img.w_emu == 2743200);
    CHECK(img.h_emu == 1828800);
    CHECK(img.parte == "ppt/media/image1.png");
}

TEST_CASE("v2: imagen con relación inexistente → aviso, sin imagen") {
    const std::string xml =
        "<?xml version=\"1.0\"?><p:sld xmlns:p=\"x\" xmlns:a=\"z\" xmlns:r=\"y\">"
        "<p:cSld><p:spTree><p:pic>"
        "<p:blipFill><a:blip r:embed=\"rNoExiste\"/></p:blipFill>"
        "<p:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"100\" cy=\"100\"/></a:xfrm></p:spPr>"
        "</p:pic></p:spTree></p:cSld></p:sld>";
    const std::string paquete = PackageConDiapositiva(xml, "<Relationships/>");
    std::vector<DiapositivaPptx> diapos;
    const InfoPptx info = LectorPptx::LeerDesdeMemoria(
        reinterpret_cast<const unsigned char*>(paquete.data()), paquete.size(),
        &diapos);
    REQUIRE(info.ok == true);
    REQUIRE(diapos.size() == 1);
    CHECK(diapos[0].imagenes.empty());
    REQUIRE(info.avisos.size() == 1);
    CHECK(info.avisos[0].find("rNoExiste") != std::string::npos);
}

TEST_CASE("v2: media ausente y media no-imagen → aviso explícito") {
    const std::string rels =
        "<?xml version=\"1.0\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rImg3\" Type=\"t/image\" Target=\"../media/faltante.png\"/></Relationships>";
    const std::string xml3 =
        "<?xml version=\"1.0\"?><p:sld xmlns:p=\"x\" xmlns:a=\"z\" xmlns:r=\"y\">"
        "<p:cSld><p:spTree><p:pic>"
        "<p:blipFill><a:blip r:embed=\"rImg3\"/></p:blipFill>"
        "<p:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"1\" cy=\"1\"/></a:xfrm></p:spPr></p:pic>"
        "</p:spTree></p:cSld></p:sld>";
    SUBCASE("media sin parte en el paquete") {
        const std::string paquete = PackageConDiapositiva(xml3, rels);
        std::vector<DiapositivaPptx> diapos;
        const InfoPptx info = LectorPptx::LeerDesdeMemoria(
            reinterpret_cast<const unsigned char*>(paquete.data()), paquete.size(),
            &diapos);
        REQUIRE(info.ok == true);
        CHECK(diapos[0].imagenes.empty());
        REQUIRE(info.avisos.size() == 1);
        CHECK(info.avisos[0].find("faltante.png") != std::string::npos);
    }
    SUBCASE("media corrupta (no es imagen)") {
        const std::string rels2 =
            "<?xml version=\"1.0\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rImg3\" Type=\"t/image\" Target=\"../media/roto.png\"/></Relationships>";
        const std::string paquete = PackageConDiapositiva(
            xml3, rels2, {{"ppt/media/roto.png", "esto no es una imagen"}});
        std::vector<DiapositivaPptx> diapos;
        const InfoPptx info = LectorPptx::LeerDesdeMemoria(
            reinterpret_cast<const unsigned char*>(paquete.data()), paquete.size(),
            &diapos);
        REQUIRE(info.ok == true);
        CHECK(diapos[0].imagenes.empty());
        REQUIRE(info.avisos.size() == 1);
        CHECK(info.avisos[0].find("no usable") != std::string::npos);
    }
}
