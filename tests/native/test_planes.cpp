// tests/native/test_planes.cpp — Tests de PlanPptx (proyección por
// archivo original, Sección 9.2) y PlanExport (exportadores, 9.3).

#include "doctest.h"
#include "fusion/core/PlanPptx.h"
#include "fusion/core/PlanExport.h"
#include "fusion/data/AhpFormat.h"

#include <string>

using namespace fusion;

namespace {
Elemento PptxElemento(const std::string& ruta, ModoPptx modo) {
    Elemento e;
    e.id = "el-pptx";
    e.tipo = TipoElemento::Pptx;
    e.ruta = ruta;
    e.modo_pptx = modo;
    return e;
}
} // namespace

// ---------------------------------------------------------------------------
// PlanPptx (Sección 9.2)
// ---------------------------------------------------------------------------

TEST_CASE("PlanPptx: modo com con PowerPoint disponible") {
    const auto r = PlanPptx::Planificar(
        PptxElemento("medios/culto.pptx", ModoPptx::Com), true);
    CHECK(r.ok);
    CHECK(r.modo_resuelto == ModoPptx::Com);
    CHECK_FALSE(r.ejecutar_macros);
    CHECK(r.ruta_resuelta == "medios/culto.pptx");
    CHECK(r.msg_error.empty());
}

TEST_CASE("PlanPptx: modo com sin PowerPoint da error explícito, sin retroceso") {
    const auto r = PlanPptx::Planificar(
        PptxElemento("medios/culto.pptx", ModoPptx::Com), false);
    CHECK_FALSE(r.ok);
    CHECK(r.msg_error.find("directo") != std::string::npos);
}

TEST_CASE("PlanPptx: modo directo no necesita PowerPoint") {
    const auto r = PlanPptx::Planificar(
        PptxElemento("medios/culto.pptx", ModoPptx::Directo), false);
    CHECK(r.ok);
    CHECK(r.modo_resuelto == ModoPptx::Directo);
    REQUIRE(r.avisos.size() == 1);
    CHECK(r.avisos[0].find("sin PowerPoint") != std::string::npos);
}

TEST_CASE("PlanPptx: un .pptm avisa y nunca ejecuta macros") {
    const auto r = PlanPptx::Planificar(
        PptxElemento("medios/con_macros.pptm", ModoPptx::Directo), false);
    CHECK(r.ok);
    CHECK_FALSE(r.ejecutar_macros);
    REQUIRE_FALSE(r.avisos.empty());
    bool avisa = false;
    for (const auto& a : r.avisos) {
        if (a.find("sin ejecutarlas") != std::string::npos) avisa = true;
    }
    CHECK(avisa);
}

TEST_CASE("PlanPptx: extensiones no soportadas se rechazan") {
    CHECK_FALSE(PlanPptx::Planificar(
        PptxElemento("medios/viejo.ppt", ModoPptx::Com), true).ok);
    CHECK_FALSE(PlanPptx::Planificar(
        PptxElemento("medios/video.mp4", ModoPptx::Com), true).ok);
    CHECK_FALSE(PlanPptx::Planificar(
        PptxElemento("medios/sinextension", ModoPptx::Com), true).ok);
}

TEST_CASE("PlanPptx: extensiones en mayúsculas se aceptan (ruta intacta)") {
    const auto r = PlanPptx::Planificar(
        PptxElemento("medios/CULTO.PPTX", ModoPptx::Directo), false);
    CHECK(r.ok);
    CHECK(r.ruta_resuelta == "medios/CULTO.PPTX");
}

TEST_CASE("PlanPptx: modo inválido, ruta vacía y tipo equivocado fallan") {
    CHECK_FALSE(PlanPptx::Planificar(
        PptxElemento("a.pptx", ModoPptx::Desconocido), true).ok);
    CHECK_FALSE(PlanPptx::Planificar(
        PptxElemento("", ModoPptx::Com), true).ok);
    Elemento texto;
    texto.id = "el-t";
    texto.tipo = TipoElemento::Texto;
    texto.ruta = "a.pptx";
    CHECK_FALSE(PlanPptx::Planificar(texto, true).ok);
}

// ---------------------------------------------------------------------------
// Integración ahp.v1 ↔ PlanPptx: el modo leído del .ahp alimenta el plan
// ---------------------------------------------------------------------------

TEST_CASE("PlanPptx: el modo del .ahp llega al planificador") {
    const std::string json = R"({
        "formato":"ahp","version":1,
        "meta":{"titulo":"Culto"},
        "escenarios":[{
            "id":"esc-1","nombre":"Predica",
            "elementos":[
                {"id":"el-1","tipo":"pptx","ruta":"predica.pptx","modo":"directo"},
                {"id":"el-2","tipo":"pptx","ruta":"diapos.pptx"}
            ]
        }]
    })";
    Programa p;
    std::string err;
    REQUIRE(AhpFormat::CargarFromString(json, &p, &err));
    REQUIRE(p.escenarios[0].elementos.size() == 2);
    CHECK(p.escenarios[0].elementos[0].modo_pptx == ModoPptx::Directo);
    // Sin "modo" → "com" (compatibilidad con archivos antiguos)
    CHECK(p.escenarios[0].elementos[1].modo_pptx == ModoPptx::Com);

    const auto r1 = PlanPptx::Planificar(p.escenarios[0].elementos[0], false);
    CHECK(r1.ok);
    const auto r2 = PlanPptx::Planificar(p.escenarios[0].elementos[1], false);
    CHECK_FALSE(r2.ok);  // modo com sin PowerPoint
}

// ---------------------------------------------------------------------------
// PlanExport (Sección 9.3)
// ---------------------------------------------------------------------------

namespace {
Programa ProgramaDePrueba() {
    Programa p;
    p.titulo = "Culto Domingo: Adoración!";
    p.fecha  = "2026-09-28";
    Escenario e; e.id = "esc-1"; e.nombre = "Bloque 1";
    Elemento t; t.id = "el-1"; t.tipo = TipoElemento::Texto;
    t.lineas = { {"linea 1", "1"}, {"linea 2", "2"}, {"linea 3", "3"} };
    Elemento v; v.id = "el-2"; v.tipo = TipoElemento::Versiculo;
    v.lineas = { {"verso", ""} };
    Elemento img; img.id = "el-3"; img.tipo = TipoElemento::Imagen;
    img.ruta = "portada.png";
    Elemento vid; vid.id = "el-4"; vid.tipo = TipoElemento::Video;
    vid.ruta = "intro.mp4";
    e.elementos = { t, v, img, vid };
    p.escenarios.push_back(e);
    return p;
}
} // namespace

TEST_CASE("PlanExport: conteo de unidades — texto por línea, medios por elemento") {
    const Programa p = ProgramaDePrueba();
    CHECK(PlanExport::ContarUnidades(p) == 6);  // 3 líneas + 1 verso + img + vid
}

TEST_CASE("PlanExport: plan de los tres formatos con avisos y nombre base") {
    const Programa p = ProgramaDePrueba();

    const auto r_pptx = PlanExport::Planificar(p, FormatoExportacion::Pptx);
    CHECK(r_pptx.ok);
    CHECK(r_pptx.unidades == 6);
    CHECK_FALSE(r_pptx.nombre_base.empty());
    CHECK_FALSE(r_pptx.avisos.empty());

    const auto r_pdf = PlanExport::Planificar(p, FormatoExportacion::Pdf);
    CHECK(r_pdf.ok);
    CHECK(r_pdf.unidades == 6);

    const auto r_png = PlanExport::Planificar(p, FormatoExportacion::Imagenes1080p);
    CHECK(r_png.ok);
    CHECK(r_png.unidades == 6);
    bool menciona_1080 = false;
    for (const auto& a : r_png.avisos) {
        if (a.find("1920x1080") != std::string::npos) menciona_1080 = true;
    }
    CHECK(menciona_1080);
}

TEST_CASE("PlanExport: el nombre base sanea caracteres ilegales y añade fecha") {
    const Programa p = ProgramaDePrueba();
    const auto r = PlanExport::Planificar(p, FormatoExportacion::Pptx);
    REQUIRE(r.ok);
    // "Culto Domingo: Adoración!" → ':' y '!' se sustituyen por '_'
    CHECK(r.nombre_base.find(':') == std::string::npos);
    CHECK(r.nombre_base.find('!') == std::string::npos);
    CHECK(r.nombre_base.find("2026-09-28") != std::string::npos);
    CHECK(r.nombre_base.find("Culto") != std::string::npos);
}

TEST_CASE("PlanExport: programa vacío y sin elementos fallan") {
    Programa vacio;
    const auto r1 = PlanExport::Planificar(vacio, FormatoExportacion::Pptx);
    CHECK_FALSE(r1.ok);
    CHECK_FALSE(r1.msg_error.empty());

    Programa sin_elementos;
    Escenario e; e.id = "esc-1"; e.nombre = "Vacio";
    sin_elementos.escenarios.push_back(e);
    const auto r2 = PlanExport::Planificar(sin_elementos,
                                           FormatoExportacion::Imagenes1080p);
    CHECK_FALSE(r2.ok);
}
