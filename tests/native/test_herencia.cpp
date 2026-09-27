// tests/native/test_herencia.cpp — Tests de herencia de temas 4 niveles
// y del informe de fidelidad (docs/agent/format_ahp_v1.md, "Temas y herencia").

#include "doctest.h"
#include "fusion/core/HerenciaTemas.h"
#include "fusion/data/AhpFormat.h"

#include <string>

using namespace fusion;

namespace {

CapaTema Capa(const std::map<std::string, std::string>& pares) {
    return CapaTema(pares.begin(), pares.end());
}

} // namespace

TEST_CASE("Herencia: la prioridad es raiz < escenario < elemento < runtime") {
    CapaTema raiz = Capa({
        {"texto.color",  "#FFFFFF"},
        {"texto.tamano", "60"},
        {"fondo.color1", "#0B1F3A"},
    });
    CapaTema escenario = Capa({ {"texto.color", "#FFCC00"} });
    CapaTema elemento  = Capa({ {"texto.tamano", "80"} });
    CapaTema runtime   = Capa({ {"fondo.color1", "#000000"} });

    const ResolucionTema r = HerenciaTemas::Resolver(
        &raiz, &escenario, &elemento, &runtime);

    REQUIRE(r.Buscar("texto.color") != nullptr);
    CHECK(*r.Buscar("texto.color") == "#FFCC00");
    CHECK(*r.Buscar("texto.tamano") == "80");
    CHECK(*r.Buscar("fondo.color1") == "#000000");
}

TEST_CASE("Herencia: informe de fidelidad enumera solo cambios efectivos") {
    CapaTema raiz = Capa({
        {"texto.color", "#FFFFFF"},
        {"texto.tamano", "60"},
    });
    // Reafirma el mismo color (no entra al informe) y cambia el tamaño.
    CapaTema escenario = Capa({
        {"texto.color", "#FFFFFF"},
        {"texto.tamano", "72"},
    });

    const ResolucionTema r = HerenciaTemas::Resolver(&raiz, &escenario, nullptr, nullptr);

    REQUIRE(r.informe.size() == 3);  // 2 altas en raiz + 1 cambio en escenario
    CHECK(r.informe[0].nivel == NivelTema::Raiz);
    CHECK(r.informe[0].propiedad == "texto.color");
    CHECK(r.informe[0].valor_anterior == "");
    CHECK(r.informe[0].valor_nuevo == "#FFFFFF");
    CHECK(r.informe[1].nivel == NivelTema::Raiz);
    CHECK(r.informe[1].propiedad == "texto.tamano");
    CHECK(r.informe[2].nivel == NivelTema::Escenario);
    CHECK(r.informe[2].propiedad == "texto.tamano");
    CHECK(r.informe[2].valor_anterior == "60");
    CHECK(r.informe[2].valor_nuevo == "72");
}

TEST_CASE("Herencia: la misma propiedad en varios niveles deja un registro por nivel") {
    CapaTema raiz = Capa({ {"texto.color", "#111111"} });
    CapaTema elemento = Capa({ {"texto.color", "#222222"} });
    CapaTema runtime  = Capa({ {"texto.color", "#333333"} });

    const ResolucionTema r = HerenciaTemas::Resolver(&raiz, nullptr, &elemento, &runtime);

    REQUIRE(r.informe.size() == 3);
    CHECK(r.informe[0].nivel == NivelTema::Raiz);
    CHECK(r.informe[1].nivel == NivelTema::Elemento);
    CHECK(r.informe[1].valor_anterior == "#111111");
    CHECK(r.informe[1].valor_nuevo == "#222222");
    CHECK(r.informe[2].nivel == NivelTema::Runtime);
    CHECK(r.informe[2].valor_anterior == "#222222");
    CHECK(r.informe[2].valor_nuevo == "#333333");
    CHECK(*r.Buscar("texto.color") == "#333333");
}

TEST_CASE("Herencia: capas nulas se ignoran y raiz vacia da resolucion vacia") {
    const ResolucionTema r = HerenciaTemas::Resolver(nullptr, nullptr, nullptr, nullptr);
    CHECK(r.estilo_resuelto.empty());
    CHECK(r.informe.empty());
}

TEST_CASE("Herencia: claves desconocidas se conservan y entran al informe") {
    CapaTema raiz = Capa({ {"clave.futura", "1"} });
    const ResolucionTema r = HerenciaTemas::Resolver(&raiz, nullptr, nullptr, nullptr);
    CHECK(*r.Buscar("clave.futura") == "1");
    REQUIRE(r.informe.size() == 1);
    CHECK(r.informe[0].propiedad == "clave.futura");
}

TEST_CASE("AplicarAEstilos: aplica texto y fondo desde el bolso resuelto") {
    CapaTema resuelto = Capa({
        {"texto.familia",           "Outfit"},
        {"texto.tamano",            "55.5"},
        {"texto.color",             "#123456"},
        {"texto.negrita",           "true"},
        {"texto.cursiva",           "false"},
        {"texto.alineacion_centro", "false"},
        {"fondo.tipo",              "gradiente"},
        {"fondo.color1",            "#0B1F3A"},
        {"fondo.color2",            "#000000"},
        {"fondo.ajuste",            "contener"},
        {"otra.clave",              "x"},   // ajena: sin efecto
    });

    EstiloTexto t;
    Fondo f;
    CHECK(AplicarAEstilos(resuelto, &t, &f));

    CHECK(t.familia == L"Outfit");
    CHECK(t.tamano == doctest::Approx(55.5f));
    CHECK(t.color.r == 0x12);
    CHECK(t.color.g == 0x34);
    CHECK(t.color.b == 0x56);
    CHECK(t.negrita == true);
    CHECK(t.cursiva == false);
    CHECK(t.alineacion_centro == false);
    CHECK(f.tipo == Fondo::Tipo::Gradiente);
    CHECK(f.color1.r == 0x0B);
    CHECK(f.color2.b == 0x00);
    CHECK(f.ajuste == AjusteImagen::Contener);
}

TEST_CASE("AplicarAEstilos: valores invalidos se ignoran y conserva el anterior") {
    CapaTema resuelto = Capa({
        {"texto.color",  "#ZZZZZZ"},   // hex inválido
        {"texto.tamano", "-3"},        // fuera de rango
        {"fondo.tipo",   "platillos"}, // tipo desconocido
        {"fondo.ajuste", "voltear"},   // ajuste desconocido
    });

    EstiloTexto t;
    t.tamano = 40;
    t.color = { 1, 2, 3 };
    Fondo f;
    f.tipo = Fondo::Tipo::Solido;
    f.ajuste = AjusteImagen::Cubrir;

    CHECK_FALSE(AplicarAEstilos(resuelto, &t, &f));
    CHECK(t.tamano == doctest::Approx(40.0f));
    CHECK(t.color.r == 1);
    CHECK(f.tipo == Fondo::Tipo::Solido);
    CHECK(f.ajuste == AjusteImagen::Cubrir);
}

TEST_CASE("AplicarAEstilos: familia y rutas con acentos UTF-8 se conservan") {
    CapaTema resuelto = Capa({
        {"texto.familia",     "Espíritu"},
        {"fondo.ruta_imagen", "fondos/año.png"},
    });
    EstiloTexto t;
    Fondo f;
    CHECK(AplicarAEstilos(resuelto, &t, &f));
    CHECK(t.familia == L"Esp\u00edritu");
    CHECK(f.ruta_imagen == L"fondos/a\u00f1o.png");
}

TEST_CASE("BibliotecaTemas: carga válida con temas parciales") {
    const std::string json = R"({
        "formato": "temas", "version": 1,
        "temas": {
            "DominicalCalido": {
                "texto.familia": "Outfit",
                "texto.color": "#FFFFFF",
                "fondo.tipo": "gradiente",
                "fondo.color1": "#0B1F3A"
            },
            "Sobrio": { "texto.color": "#DDDDDD" }
        }
    })";
    std::map<std::string, CapaTema> temas;
    std::string err;
    CHECK(BibliotecaTemas::CargarFromString(json, &temas, &err));
    CHECK(temas.size() == 2);
    REQUIRE(temas.count("DominicalCalido") == 1);
    CHECK(temas.at("DominicalCalido").at("texto.color") == "#FFFFFF");
    CHECK(temas.at("Sobrio").size() == 1);
}

TEST_CASE("BibliotecaTemas: JSON inválido y versión futura fallan") {
    std::map<std::string, CapaTema> temas;
    std::string err;

    CHECK_FALSE(BibliotecaTemas::CargarFromString("{ no json", &temas, &err));

    const std::string futuro = R"({"formato":"temas","version":2,"temas":{}})";
    CHECK_FALSE(BibliotecaTemas::CargarFromString(futuro, &temas, &err));
    CHECK(err.find("futura") != std::string::npos);
}

TEST_CASE("BibliotecaTemas: tema no-objeto y propiedad no-string fallan") {
    std::map<std::string, CapaTema> temas;
    std::string err;

    CHECK_FALSE(BibliotecaTemas::CargarFromString(
        R"({"version":1,"temas":{"Malo": [1,2]}})", &temas, &err));
    CHECK_FALSE(BibliotecaTemas::CargarFromString(
        R"({"version":1,"temas":{"Malo": {"texto.tamano": 60}}})", &temas, &err));
    CHECK(temas.empty());
}

TEST_CASE("Integración ahp.v1 + biblioteca + runtime: resolución completa") {
    // Temas nombrados de la biblioteca.
    const std::string json_temas = R"({
        "version": 1,
        "temas": {
            "DominicalCalido": { "texto.color": "#FFFFFF", "fondo.color1": "#5A3E2B", "texto.tamano": "60" },
            "Sobrio":          { "texto.color": "#EEEEEE", "fondo.color1": "#101010" },
            "Contraste":       { "texto.color": "#FFFF00", "texto.tamano": "90" }
        }
    })";
    std::map<std::string, CapaTema> temas;
    std::string err;
    REQUIRE(BibliotecaTemas::CargarFromString(json_temas, &temas, &err));

    // Programa con 3 niveles referenciados por nombre.
    const std::string json_prog = R"({
        "formato":"ahp","version":1,
        "meta":{"titulo":"Culto","tema_raiz":"DominicalCalido"},
        "escenarios":[{
            "id":"esc-1","nombre":"Alabanza","tema":"Sobrio",
            "elementos":[{
                "id":"el-1","tipo":"texto","titulo":"Título",
                "lineas":[{"texto":"Santo, Santo, Santo"}],
                "tema_override":"Contraste"
            }]
        }]
    })";
    Programa p;
    REQUIRE(AhpFormat::CargarFromString(json_prog, &p, &err));

    REQUIRE(!p.escenarios.empty());
    REQUIRE(!p.escenarios[0].elementos.empty());

    // Capas: raíz (meta.tema_raiz), escenario, elemento. El nivel 4
    // (runtime) lo aporta Sesion.tema_runtime; aquí simulado en caliente.
    CapaTema runtime = Capa({ {"texto.color", "#00FF00"} });

    const ResolucionTema r = HerenciaTemas::Resolver(
        &temas[p.tema_raiz],
        &temas[p.escenarios[0].tema],
        &temas[p.escenarios[0].elementos[0].tema_override],
        &runtime);

    CHECK(*r.Buscar("texto.color") == "#00FF00");
    CHECK(*r.Buscar("texto.tamano") == "90");          // del override del elemento
    CHECK(*r.Buscar("fondo.color1") == "#101010");     // del tema del escenario

    // El informe debe tener, en orden: raiz (3 altas), escenario (2
    // cambios), elemento (2 cambios), runtime (1 cambio).
    std::size_t por_nivel[4] = {0, 0, 0, 0};
    for (const auto& reg : r.informe) ++por_nivel[static_cast<int>(reg.nivel)];
    CHECK(por_nivel[0] == 3);
    CHECK(por_nivel[1] == 2);
    CHECK(por_nivel[2] == 2);
    CHECK(por_nivel[3] == 1);
}
