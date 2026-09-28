// tests/native/test_render_directo.cpp — Tests del render directo (9.2):
// decodificador UTF-8, construcción del plan portable y despacho a
// Renderer con un mock de grabación. La parte D2D (RasterizadorDirectoD2D)
// solo compila en Windows (CI): aquí se prueba todo lo portable.

#include "doctest.h"
#include "fusion/core/RenderDirecto.h"
#include "fusion/core/HerenciaTemas.h"

#include <string>
#include <vector>

using namespace fusion;

namespace {

CapaTema Capa(const std::map<std::string, std::string>& pares) {
    return CapaTema(pares.begin(), pares.end());
}

// Renderer que graba las llamadas para verificar el despacho del plan.
class RendererGrabador : public Renderer {
public:
    struct TextoLlamado {
        std::wstring texto;
        EstiloTexto estilo;
        float x, y, w, h;
    };

    bool Inicializar(void*) override { return true; }
    void Liberar() override {}
    void Limpiar() override {}
    void Presentar() override {}

    void DibujarFondo(const Fondo& f) override { fondos.push_back(f); }

    void DibujarTexto(const std::wstring& texto, const EstiloTexto& estilo,
                      float x, float y, float w, float h) override {
        textos.push_back({texto, estilo, x, y, w, h});
    }

    void DibujarImagen(const std::wstring&, float, float, float, float,
                       AjusteImagen) override {
        ++imagenes;
    }

    std::vector<Fondo> fondos;
    std::vector<TextoLlamado> textos;
    int imagenes = 0;
};

DiapositivaPptx DiapoEjemplo() {
    DiapositivaPptx d;
    d.indice = 1;
    d.titulo = "Santo, Santo, Santo";
    d.parrafos = {"Primera estrofa", "Segunda estrofa", "Tercera estrofa"};
    return d;
}

} // namespace

// ---------------------------------------------------------------------------
// Utf8AUtf16
// ---------------------------------------------------------------------------

TEST_CASE("Utf8AUtf16: ASCII y acentos españoles") {
    CHECK(Utf8AUtf16("") == L"");
    CHECK(Utf8AUtf16("Hola 123") == L"Hola 123");
    // ó = U+00F3 (C3 B3)
    const std::wstring cancion = Utf8AUtf16("Canci\xc3\xb3n");
    REQUIRE(cancion.size() == 7);
    CHECK(cancion[5] == 0x00F3);
    // ñ = U+00F1 (C3 B1), Á = U+00C1
    const std::wstring anho = Utf8AUtf16("\xc3\x81\xc3\xb1o");
    REQUIRE(anho.size() == 3);
    CHECK(anho[0] == 0x00C1);
    CHECK(anho[1] == 0x00F1);
}

TEST_CASE("Utf8AUtf16: multibyte 3 y 4 bytes (CJK y emoji como par suplente)") {
    // 中 = U+4E2D (E4 B8 AD)
    const std::wstring cjk = Utf8AUtf16("\xe4\xb8\xad");
    REQUIRE(cjk.size() == 1);
    CHECK(cjk[0] == 0x4E2D);
    // U+1F600 (F0 9F 98 80) → par suplente D800+ / DC00+ en UTF-16.
    const std::wstring emoji = Utf8AUtf16("\xf0\x9f\x98\x80");
    REQUIRE(emoji.size() == 2);
    CHECK(emoji[0] == static_cast<wchar_t>(0xD83D));
    CHECK(emoji[1] == static_cast<wchar_t>(0xDE00));
}

TEST_CASE("Utf8AUtf16: bytes inválidos, truncados y sobrecodificados dan U+FFFD sin tragarse el resto") {
    const wchar_t mal = 0xFFFD;
    // Guía de 3 bytes seguido de ASCII: U+FFFD y la 'A' sobrevive.
    const std::wstring roto = Utf8AUtf16("\xe0"
                                         "A");
    REQUIRE(roto.size() == 2);
    CHECK(roto[0] == mal);
    CHECK(roto[1] == L'A');
    // Secuencia truncada al final.
    const std::wstring truncado = Utf8AUtf16("fin\xc3");
    REQUIRE(truncado.size() == 4);
    CHECK(truncado[3] == mal);
    // Punto de código > U+10FFFF (F4 90 80 80).
    const std::wstring pasado = Utf8AUtf16("\xf4\x90\x80\x80");
    CHECK(pasado.size() >= 1);
    CHECK(pasado[0] == mal);
    // Suplente codificado en UTF-8 (ED A0 80 = U+D800).
    const std::wstring suplente = Utf8AUtf16("\xed\xa0\x80");
    CHECK(suplente.size() == 1);
    CHECK(suplente[0] == mal);
    // Continuación suelta.
    const std::wstring suelta = Utf8AUtf16("\x80");
    REQUIRE(suelta.size() == 1);
    CHECK(suelta[0] == mal);
}

// ---------------------------------------------------------------------------
// ConstruirPlan
// ---------------------------------------------------------------------------

TEST_CASE("RenderDirecto: plan con título y párrafos, bandas y slots correctos") {
    // Tema vacío: estilos por defecto.
    const ResolucionTema tema = HerenciaTemas::Resolver(
        nullptr, nullptr, nullptr, nullptr, nullptr);

    // Tema con estilo real para verificar el traspaso.
    const CapaTema estilo_tema = Capa({
        {"texto.color", "#FF8800"},
        {"fondo.tipo",  "gradiente"},
        {"fondo.color1","#0B1F3A"},
        {"fondo.color2","#000000"},
    });
    const ResolucionTema tema2 = HerenciaTemas::Resolver(
        &estilo_tema, nullptr, nullptr, nullptr, nullptr);

    PlanRenderDirecto plan;
    const ResultadoRenderDirecto r = RenderDirecto::ConstruirPlan(
        DiapoEjemplo(), tema2, 16.0f / 9.0f, &plan);

    REQUIRE(r.ok);
    CHECK(r.avisos.empty());
    CHECK(plan.aspecto == doctest::Approx(16.0f / 9.0f));
    CHECK(plan.fondo.tipo == Fondo::Tipo::Gradiente);
    CHECK(plan.fondo.color1.r == 0x0B);
    CHECK(plan.fondo.color2.b == 0x00);

    // 1 título + 3 párrafos, en ese orden.
    REQUIRE(plan.pasos.size() == 4);
    CHECK(plan.pasos[0].tipo == PasoDibujo::Tipo::Titulo);
    CHECK(plan.pasos[1].tipo == PasoDibujo::Tipo::Cuerpo);

    // Banda del título.
    CHECK(plan.pasos[0].x == doctest::Approx(0.08f));
    CHECK(plan.pasos[0].y == doctest::Approx(0.10f));
    CHECK(plan.pasos[0].w == doctest::Approx(0.84f));
    CHECK(plan.pasos[0].h == doctest::Approx(0.18f));
    CHECK(plan.pasos[0].texto == L"Santo, Santo, Santo");
    // El estilo del tema llega al paso (color 0xFF,0x88,0x00).
    CHECK(plan.pasos[0].estilo.color.r == 0xFF);
    CHECK(plan.pasos[0].estilo.color.g == 0x88);

    // Banda del cuerpo: y=0.34 h=0.56 repartido en 3 slots iguales.
    const float slot = 0.56f / 3.0f;
    for (int i = 1; i <= 3; ++i) {
        const PasoDibujo& p = plan.pasos[static_cast<size_t>(i)];
        CHECK(p.x == doctest::Approx(0.08f));
        CHECK(p.w == doctest::Approx(0.84f));
        CHECK(p.y == doctest::Approx(0.34f + (i - 1) * slot));
        CHECK(p.h == doctest::Approx(slot));
    }
    CHECK(plan.pasos[1].texto == L"Primera estrofa");
    CHECK(plan.pasos[3].texto == L"Tercera estrofa");

    // El tema vacío (primer plan) no debe romper nada: fondo negro sólido.
    PlanRenderDirecto plan_vacio;
    const ResultadoRenderDirecto rv =
        RenderDirecto::ConstruirPlan(DiapoEjemplo(), tema, 0.0f, &plan_vacio);
    REQUIRE(rv.ok);
    CHECK(plan_vacio.fondo.tipo == Fondo::Tipo::Solido);
    CHECK(plan_vacio.fondo.color1.r == 0);
    CHECK(plan_vacio.aspecto == doctest::Approx(16.0f / 9.0f));  // 0 → 16:9
}

TEST_CASE("RenderDirecto: sin título el cuerpo ocupa la banda completa") {
    DiapositivaPptx d;
    d.indice = 1;
    d.parrafos = {"Solo cuerpo"};
    const ResolucionTema tema = HerenciaTemas::Resolver(
        nullptr, nullptr, nullptr, nullptr, nullptr);

    PlanRenderDirecto plan;
    REQUIRE(RenderDirecto::ConstruirPlan(d, tema, 1.3333f, &plan).ok);

    REQUIRE(plan.pasos.size() == 1);
    CHECK(plan.pasos[0].tipo == PasoDibujo::Tipo::Cuerpo);
    CHECK(plan.pasos[0].y == doctest::Approx(0.10f));
    CHECK(plan.pasos[0].h == doctest::Approx(0.80f));
    CHECK(plan.aspecto == doctest::Approx(1.3333f));
}

TEST_CASE("RenderDirecto: diapositiva vacía → solo fondo; parrafos truncados con aviso") {
    const ResolucionTema tema = HerenciaTemas::Resolver(
        nullptr, nullptr, nullptr, nullptr, nullptr);

    // Vacía: sin pasos.
    DiapositivaPptx vacia;
    PlanRenderDirecto plan;
    const ResultadoRenderDirecto r =
        RenderDirecto::ConstruirPlan(vacia, tema, 16.0f / 9.0f, &plan);
    REQUIRE(r.ok);
    CHECK(plan.pasos.empty());

    // 5 párrafos con max 3: 3 pasos y aviso explícito.
    DiapositivaPptx d;
    d.indice = 1;
    for (int i = 1; i <= 5; ++i)
        d.parrafos.push_back("P" + std::to_string(i));
    OpcionesRenderDirecto ops;
    ops.max_parrafos = 3;
    PlanRenderDirecto plan2;
    const ResultadoRenderDirecto r2 =
        RenderDirecto::ConstruirPlan(d, tema, 16.0f / 9.0f, &plan2, ops);
    REQUIRE(r2.ok);
    REQUIRE(plan2.pasos.size() == 3);  // solo el cuerpo (sin título)
    REQUIRE(r2.avisos.size() == 1);
    CHECK(r2.avisos[0].find("3") != std::string::npos);
    CHECK(plan2.pasos[0].texto == L"P1");
    CHECK(plan2.pasos[2].texto == L"P3");

    // out == nullptr → error explícito.
    ResultadoRenderDirecto r3 =
        RenderDirecto::ConstruirPlan(d, tema, 16.0f / 9.0f, nullptr);
    CHECK_FALSE(r3.ok);
    CHECK_FALSE(r3.msg_error.empty());
}

TEST_CASE("RenderDirecto: tema con valores inválidos y fondo imagen avisan") {
    const CapaTema tema_malo = Capa({
        {"texto.color", "#ZZZZZZ"},      // hex inválido
        {"fondo.tipo",  "platillos"},    // tipo desconocido
    });
    const ResolucionTema tema = HerenciaTemas::Resolver(
        &tema_malo, nullptr, nullptr, nullptr, nullptr);

    PlanRenderDirecto plan;
    const ResultadoRenderDirecto r =
        RenderDirecto::ConstruirPlan(DiapoEjemplo(), tema, 16.0f / 9.0f, &plan);
    REQUIRE(r.ok);
    // Los valores inválidos se ignoran (estilo por defecto), con aviso.
    REQUIRE(r.avisos.size() == 1);
    CHECK(r.avisos[0].find("inválidos") != std::string::npos);
    CHECK(plan.fondo.tipo == Fondo::Tipo::Solido);  // default conservado

    // Fondo imagen en el tema: aviso específico y cae a sólido color1.
    const CapaTema tema_img = Capa({
        {"fondo.tipo",        "imagen"},
        {"fondo.color1",      "#101010"},
        {"fondo.ruta_imagen", "fondo.png"},
    });
    const ResolucionTema tema2 = HerenciaTemas::Resolver(
        &tema_img, nullptr, nullptr, nullptr, nullptr);
    PlanRenderDirecto plan2;
    const ResultadoRenderDirecto r2 =
        RenderDirecto::ConstruirPlan(DiapoEjemplo(), tema2, 16.0f / 9.0f, &plan2);
    REQUIRE(r2.ok);
    REQUIRE(r2.avisos.size() == 1);
    CHECK(r2.avisos[0].find("imagen") != std::string::npos);
    CHECK(plan2.fondo.tipo == Fondo::Tipo::Solido);
    CHECK(plan2.fondo.color1.r == 0x10);
}

TEST_CASE("RenderDirecto: aspecto saneado") {
    const ResolucionTema tema = HerenciaTemas::Resolver(
        nullptr, nullptr, nullptr, nullptr, nullptr);
    DiapositivaPptx d;
    PlanRenderDirecto plan;

    REQUIRE(RenderDirecto::ConstruirPlan(d, tema, -1.0f, &plan).ok);
    CHECK(plan.aspecto == doctest::Approx(16.0f / 9.0f));

    REQUIRE(RenderDirecto::ConstruirPlan(d, tema, 10.0f, &plan).ok);
    CHECK(plan.aspecto == doctest::Approx(4.0f));   // clamp superior

    REQUIRE(RenderDirecto::ConstruirPlan(d, tema, 0.1f, &plan).ok);
    CHECK(plan.aspecto == doctest::Approx(0.5f));   // clamp inferior
}

// ---------------------------------------------------------------------------
// DibujarPlan
// ---------------------------------------------------------------------------

TEST_CASE("DibujarPlan: despacha fondo y textos escalados al objetivo") {
    PlanRenderDirecto plan;
    plan.aspecto = 16.0f / 9.0f;
    plan.fondo.tipo = Fondo::Tipo::Solido;
    plan.fondo.color1 = {0, 1, 2};

    PasoDibujo titulo;
    titulo.tipo = PasoDibujo::Tipo::Titulo;
    titulo.x = 0.10f; titulo.y = 0.20f; titulo.w = 0.80f; titulo.h = 0.15f;
    titulo.texto = L"T\u00edtulo";
    plan.pasos.push_back(titulo);

    PasoDibujo cuerpo;
    cuerpo.tipo = PasoDibujo::Tipo::Cuerpo;
    cuerpo.x = 0.10f; cuerpo.y = 0.40f; cuerpo.w = 0.80f; cuerpo.h = 0.50f;
    cuerpo.texto = L"Cuerpo";
    plan.pasos.push_back(cuerpo);

    RendererGrabador mock;
    DibujarPlan(&mock, plan, 1920.0f, 1080.0f);

    // Fondo primero, exactamente el del plan.
    REQUIRE(mock.fondos.size() == 1);
    CHECK(mock.fondos[0].tipo == Fondo::Tipo::Solido);
    CHECK(mock.fondos[0].color1.b == 2);

    // Textos: 2, en orden, escalados a 1920x1080.
    REQUIRE(mock.textos.size() == 2);
    CHECK(mock.textos[0].texto == L"T\u00edtulo");
    CHECK(mock.textos[0].x == doctest::Approx(0.10f * 1920.0f));
    CHECK(mock.textos[0].y == doctest::Approx(0.20f * 1080.0f));
    CHECK(mock.textos[0].w == doctest::Approx(0.80f * 1920.0f));
    CHECK(mock.textos[0].h == doctest::Approx(0.15f * 1080.0f));
    CHECK(mock.textos[1].texto == L"Cuerpo");
    CHECK(mock.textos[1].y == doctest::Approx(0.40f * 1080.0f));
    CHECK(mock.imagenes == 0);

    // Renderer nulo / plan vacío: no-op sin crash.
    DibujarPlan(nullptr, plan, 1920.0f, 1080.0f);
    RendererGrabador mock2;
    PlanRenderDirecto vacio;
    DibujarPlan(&mock2, vacio, 1920.0f, 1080.0f);
    CHECK(mock2.fondos.size() == 1);  // el fondo siempre se dibuja
    CHECK(mock2.textos.empty());
}
