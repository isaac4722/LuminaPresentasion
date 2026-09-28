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

// ---------------------------------------------------------------------------
// Letterbox (encaje del plan en el objetivo)
// ---------------------------------------------------------------------------

TEST_CASE("DibujarPlan: letterbox 4:3 en objetivo 16:9 centra el contenido") {
    PlanRenderDirecto plan;
    plan.aspecto = 4.0f / 3.0f;
    plan.fondo.tipo = Fondo::Tipo::Solido;
    plan.fondo.color1 = {10, 20, 30};

    // Un paso que cubre TODA la diapositiva (0,0,1,1).
    PasoDibujo cuerpo;
    cuerpo.tipo = PasoDibujo::Tipo::Cuerpo;
    cuerpo.x = 0.0f; cuerpo.y = 0.0f; cuerpo.w = 1.0f; cuerpo.h = 1.0f;
    cuerpo.texto = L"Diapositiva entera";
    plan.pasos.push_back(cuerpo);

    RendererGrabador mock;
    DibujarPlan(&mock, plan, 1920.0f, 1080.0f);

    // El fondo cubre el objetivo completo (las bandas quedan con el tema).
    REQUIRE(mock.fondos.size() == 1);

    // El paso queda dentro del rect de contenido: 1440x1080 centrado.
    REQUIRE(mock.textos.size() == 1);
    CHECK(mock.textos[0].x == doctest::Approx(240.0f).epsilon(0.01));
    CHECK(mock.textos[0].y == doctest::Approx(0.0f).epsilon(0.01));
    CHECK(mock.textos[0].w == doctest::Approx(1440.0f).epsilon(0.01));
    CHECK(mock.textos[0].h == doctest::Approx(1080.0f).epsilon(0.01));
}

TEST_CASE("DibujarPlan: letterbox 16:9 en objetivo 4:3 deja bandas arriba y "
          "abajo") {
    PlanRenderDirecto plan;
    plan.aspecto = 16.0f / 9.0f;

    PasoDibujo cuerpo;
    cuerpo.tipo = PasoDibujo::Tipo::Cuerpo;
    cuerpo.x = 0.0f; cuerpo.y = 0.0f; cuerpo.w = 1.0f; cuerpo.h = 1.0f;
    cuerpo.texto = L"Completa";
    plan.pasos.push_back(cuerpo);

    RendererGrabador mock;
    DibujarPlan(&mock, plan, 1024.0f, 768.0f);

    // Contenido: 1024x576 centrado verticalmente (bandas de 96 arriba y
    // abajo).
    REQUIRE(mock.textos.size() == 1);
    CHECK(mock.textos[0].x == doctest::Approx(0.0f).epsilon(0.01));
    CHECK(mock.textos[0].y == doctest::Approx(96.0f).epsilon(0.01));
    CHECK(mock.textos[0].w == doctest::Approx(1024.0f).epsilon(0.01));
    CHECK(mock.textos[0].h == doctest::Approx(576.0f).epsilon(0.01));
}

TEST_CASE("DibujarPlan: aspecto invalido degrada a objetivo completo") {
    PlanRenderDirecto plan;
    plan.aspecto = 0.0f;  // paquete sin sldSz mal construido a mano

    PasoDibujo cuerpo;
    cuerpo.tipo = PasoDibujo::Tipo::Cuerpo;
    cuerpo.x = 0.1f; cuerpo.y = 0.1f; cuerpo.w = 0.8f; cuerpo.h = 0.8f;
    cuerpo.texto = L"X";
    plan.pasos.push_back(cuerpo);

    RendererGrabador mock;
    DibujarPlan(&mock, plan, 800.0f, 600.0f);

    REQUIRE(mock.textos.size() == 1);
    CHECK(mock.textos[0].x == doctest::Approx(0.1f * 800.0f).epsilon(0.01));
    CHECK(mock.textos[0].y == doctest::Approx(0.1f * 600.0f).epsilon(0.01));
    CHECK(mock.textos[0].w == doctest::Approx(0.8f * 800.0f).epsilon(0.01));
    CHECK(mock.textos[0].h == doctest::Approx(0.8f * 600.0f).epsilon(0.01));
}

TEST_CASE("DibujarPlan: ConstruirPlan con 4:3 y despacho letterbox e2e") {
    const ResolucionTema tema = HerenciaTemas::Resolver(
        nullptr, nullptr, nullptr, nullptr, nullptr);

    PlanRenderDirecto plan;
    REQUIRE(RenderDirecto::ConstruirPlan(DiapoEjemplo(), tema, 4.0f / 3.0f,
                                         &plan)
                .ok);
    CHECK(plan.aspecto == doctest::Approx(4.0f / 3.0f));

    RendererGrabador mock;
    DibujarPlan(&mock, plan, 1920.0f, 1080.0f);

    REQUIRE(mock.fondos.size() == 1);
    // Título + 3 párrafos, todos dentro del rect 1440x1080 en x=240.
    REQUIRE(mock.textos.size() == 4);
    for (const auto& t : mock.textos) {
        CHECK(t.x >= 240.0f - 0.01f);
        CHECK(t.x + t.w <= 240.0f + 1440.0f + 0.01f);
        CHECK(t.y >= -0.01f);
        CHECK(t.y + t.h <= 1080.0f + 0.01f);
    }
}

// ---------------------------------------------------------------------------
// Auto-ajuste del tamaño de texto y escala por resolución
// ---------------------------------------------------------------------------

TEST_CASE("ConstruirPlan: parrafo largo reduce el tamano del cuerpo con aviso") {
    const ResolucionTema tema = HerenciaTemas::Resolver(
        nullptr, nullptr, nullptr, nullptr, nullptr);

    // Control: un párrafo corto se queda con el tamaño del tema (60).
    DiapositivaPptx corto;
    corto.parrafos = {"Solo una linea corta"};
    PlanRenderDirecto plan_c;
    const ResultadoRenderDirecto r_c =
        RenderDirecto::ConstruirPlan(corto, tema, 16.0f / 9.0f, &plan_c);
    REQUIRE(r_c.ok);
    REQUIRE(plan_c.pasos.size() == 1);
    CHECK(plan_c.pasos[0].estilo.tamano == doctest::Approx(60.0f));
    CHECK(r_c.avisos.empty());

    // Largo: una sola línea no basta; el tamaño baja (x0.90) hasta que
    // la envoltura estimada cabe en el slot (banda completa 0.80x1080
    // al no haber título).
    DiapositivaPptx largo;
    std::string relleno;
    for (int i = 0; i < 90; ++i) relleno += "palabra ";
    largo.parrafos.push_back(relleno);
    PlanRenderDirecto plan;
    const ResultadoRenderDirecto r =
        RenderDirecto::ConstruirPlan(largo, tema, 16.0f / 9.0f, &plan);
    REQUIRE(r.ok);
    REQUIRE(plan.pasos.size() == 1);
    CHECK(plan.pasos[0].estilo.tamano < 60.0f);
    CHECK(plan.pasos[0].estilo.tamano >= 0.011f * 1080.0f);
    bool aviso_reduccion = false;
    for (const auto& a : r.avisos)
        if (a.find("cuerpo se redujo") != std::string::npos)
            aviso_reduccion = true;
    CHECK(aviso_reduccion);
}

TEST_CASE("ConstruirPlan: doce parrafos reparten el cuerpo y reducen el "
          "tamano") {
    const ResolucionTema tema = HerenciaTemas::Resolver(
        nullptr, nullptr, nullptr, nullptr, nullptr);

    DiapositivaPptx d;
    for (int i = 1; i <= 12; ++i)
        d.parrafos.push_back("Parrafo " + std::to_string(i));
    PlanRenderDirecto plan;
    const ResultadoRenderDirecto r =
        RenderDirecto::ConstruirPlan(d, tema, 16.0f / 9.0f, &plan);
    REQUIRE(r.ok);
    REQUIRE(plan.pasos.size() == 12);

    // Slot de 0.56x1080/12 = 48.24 px: una línea exige <= 35.7 pt.
    CHECK(plan.pasos[0].estilo.tamano < 60.0f);
    CHECK(plan.pasos[0].estilo.tamano >= 0.011f * 1080.0f);
    CHECK(r.avisos.size() == 1);  // solo la reducción (no hubo recorte)
    CHECK(r.avisos[0].find("cuerpo se redujo") != std::string::npos);

    // Todos los pasos comparten el tamaño ajustado.
    for (const auto& p : plan.pasos)
        CHECK(p.estilo.tamano == plan.pasos[0].estilo.tamano);
}

TEST_CASE("ConstruirPlan: titulo largo se reduce y avisa") {
    const ResolucionTema tema = HerenciaTemas::Resolver(
        nullptr, nullptr, nullptr, nullptr, nullptr);

    DiapositivaPptx d;
    d.titulo = std::string(120, 'x');  // 120 caracteres, banda de 0.18x1080
    PlanRenderDirecto plan;
    const ResultadoRenderDirecto r =
        RenderDirecto::ConstruirPlan(d, tema, 16.0f / 9.0f, &plan);
    REQUIRE(r.ok);
    REQUIRE(plan.pasos.size() == 1);
    CHECK(plan.pasos[0].tipo == PasoDibujo::Tipo::Titulo);
    CHECK(plan.pasos[0].estilo.tamano < 60.0f);
    CHECK(r.avisos.size() == 1);
    CHECK(r.avisos[0].find("título se redujo") != std::string::npos);
}

TEST_CASE("ConstruirPlan: texto saturado llega al tamano minimo") {
    const ResolucionTema tema = HerenciaTemas::Resolver(
        nullptr, nullptr, nullptr, nullptr, nullptr);

    DiapositivaPptx d;
    for (int i = 0; i < 200; ++i)
        d.parrafos.push_back("linea de texto para saturar el area");
    OpcionesRenderDirecto ops;
    ops.max_parrafos = 200;

    PlanRenderDirecto plan;
    const ResultadoRenderDirecto r =
        RenderDirecto::ConstruirPlan(d, tema, 16.0f / 9.0f, &plan, ops);
    REQUIRE(r.ok);
    REQUIRE(plan.pasos.size() == 200);
    // Mínimo del contrato: max(9, 1.1 % de 1080) = 11.88 px.
    CHECK(plan.pasos[0].estilo.tamano ==
          doctest::Approx(0.011f * 1080.0f).epsilon(0.01));
}

TEST_CASE("DibujarPlan: reescala el tamano de fuente al objetivo (alto/1080)") {
    PlanRenderDirecto plan;
    plan.aspecto = 16.0f / 9.0f;
    PasoDibujo cuerpo;
    cuerpo.tipo = PasoDibujo::Tipo::Cuerpo;
    cuerpo.x = 0.08f; cuerpo.y = 0.10f; cuerpo.w = 0.84f; cuerpo.h = 0.80f;
    cuerpo.texto = L"Texto";
    cuerpo.estilo.tamano = 60.0f;
    plan.pasos.push_back(cuerpo);

    // Objetivo 1080p: identidad.
    RendererGrabador m1080;
    DibujarPlan(&m1080, plan, 1920.0f, 1080.0f);
    REQUIRE(m1080.textos.size() == 1);
    CHECK(m1080.textos[0].estilo.tamano == doctest::Approx(60.0f));

    // Objetivo 800x450: 450/1080 = 0.4167 -> 25 px.
    RendererGrabador m450;
    DibujarPlan(&m450, plan, 800.0f, 450.0f);
    REQUIRE(m450.textos.size() == 1);
    CHECK(m450.textos[0].estilo.tamano == doctest::Approx(25.0f).epsilon(0.01));
}
