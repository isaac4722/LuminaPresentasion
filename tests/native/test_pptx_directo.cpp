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
