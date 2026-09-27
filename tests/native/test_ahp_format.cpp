// tests/native/test_ahp_format.cpp — Tests del parser ahp.v1

#include "doctest.h"
#include "fusion/data/AhpFormat.h"
#include <string>

using namespace fusion;

TEST_CASE("AhpFormat.CargarFromString válido") {
    const std::string json = R"({
        "formato":"ahp","version":1,
        "meta":{"titulo":"Culto Test"},
        "escenarios":[{"id":"esc-1","nombre":"Bienvenida","elementos":[]}]
    })";
    Programa p;
    std::string err;
    bool ok = AhpFormat::CargarFromString(json, &p, &err);
    CHECK(ok);
    CHECK(p.titulo == "Culto Test");
    CHECK(p.escenarios.size() == 1);
    CHECK(p.escenarios[0].nombre == "Bienvenida");
}

TEST_CASE("AhpFormat.CargarFromString versión futura falla") {
    const std::string json = R"({
        "formato":"ahp","version":99,
        "meta":{},"escenarios":[]
    })";
    Programa p;
    std::string err;
    bool ok = AhpFormat::CargarFromString(json, &p, &err);
    CHECK(!ok);
    CHECK(!err.empty());
}

TEST_CASE("AhpFormat.Serializar luego CargarFromString redondea") {
    Programa p;
    p.titulo = "Ida y vuelta";
    Escenario e; e.id = "esc-1"; e.nombre = "Escena 1";
    Elemento el; el.id = "el-1"; el.tipo = TipoElemento::Texto;
    el.titulo = "Hola";
    el.lineas.push_back({"Hola mundo", "1"});
    e.elementos.push_back(el);
    p.escenarios.push_back(e);

    std::string s = AhpFormat::Serializar(p);
    Programa p2;
    std::string err;
    REQUIRE(AhpFormat::CargarFromString(s, &p2, &err));
    CHECK(p2.titulo == p.titulo);
    CHECK(p2.escenarios.size() == 1);
    CHECK(p2.escenarios[0].elementos.size() == 1);
    CHECK(p2.escenarios[0].elementos[0].lineas[0].texto == "Hola mundo");
}

TEST_CASE("AhpFormat.Validar detecta ids de escenario duplicados") {
    Programa p;
    Escenario e1; e1.id = "dup"; e1.nombre = "A";
    Escenario e2; e2.id = "dup"; e2.nombre = "B";
    p.escenarios.push_back(e1);
    p.escenarios.push_back(e2);
    std::string err;
    CHECK_FALSE(AhpFormat::Validar(p, &err));
}

TEST_CASE("AhpFormat: redondeo de campos que antes se perdían al guardar") {
    // Regresión: modo_versiculo, ajuste, bucle y audio no se serializaban
    // y el guardado los reseteaba silenciosamente a los valores por
    // defecto.
    Programa p;
    p.titulo = "Campos perdidos";
    Escenario e; e.id = "esc-1"; e.nombre = "Escena 1";

    Elemento v; v.id = "el-v"; v.tipo = TipoElemento::Versiculo;
    v.cita = "Salmo 100:4";
    v.texto_versiculo = "Entrad por sus puertas...";
    v.modo_versiculo = ModoVersiculo::Tercio;

    Elemento img; img.id = "el-i"; img.tipo = TipoElemento::Imagen;
    img.ruta = "fondos/portada.png";
    img.ajuste = AjusteImagen::Contener;

    Elemento vid; vid.id = "el-vid"; vid.tipo = TipoElemento::Video;
    vid.ruta = "videos/intro.mp4";
    vid.bucle_video = true;
    vid.audio_video = false;

    e.elementos = { v, img, vid };
    p.escenarios.push_back(e);

    const std::string s = AhpFormat::Serializar(p);
    Programa p2;
    std::string err;
    REQUIRE(AhpFormat::CargarFromString(s, &p2, &err));
    REQUIRE(p2.escenarios[0].elementos.size() == 3);
    CHECK(p2.escenarios[0].elementos[0].modo_versiculo == ModoVersiculo::Tercio);
    CHECK(p2.escenarios[0].elementos[1].ajuste == AjusteImagen::Contener);
    CHECK(p2.escenarios[0].elementos[2].bucle_video == true);
    CHECK(p2.escenarios[0].elementos[2].audio_video == false);
}

TEST_CASE("AhpFormat.Validar rechaza tipo de elemento desconocido") {
    const std::string json = R"({
        "formato":"ahp","version":1,
        "meta":{"titulo":"T"},
        "escenarios":[{"id":"esc-1","nombre":"E","elementos":[
            {"id":"el-1","tipo":"sermon","titulo":"X"}
        ]}]
    })";
    Programa p;
    std::string err;
    REQUIRE(AhpFormat::CargarFromString(json, &p, &err));
    err.clear();
    CHECK_FALSE(AhpFormat::Validar(p, &err));
    CHECK(err.find("desconocido") != std::string::npos);
}

TEST_CASE("AhpFormat: pptx con modo inválido se rechaza en Validar") {
    const std::string json = R"({
        "formato":"ahp","version":1,
        "meta":{"titulo":"T"},
        "escenarios":[{"id":"esc-1","nombre":"E","elementos":[
            {"id":"el-1","tipo":"pptx","ruta":"a.pptx","modo":"automatico"}
        ]}]
    })";
    Programa p;
    std::string err;
    REQUIRE(AhpFormat::CargarFromString(json, &p, &err));
    err.clear();
    CHECK_FALSE(AhpFormat::Validar(p, &err));
    CHECK(err.find("pptx") != std::string::npos);
}

TEST_CASE("AhpFormat: pptx modo com/directo redondea por Serializar/Cargar") {
    Programa p;
    p.titulo = "Pptx";
    Escenario e; e.id = "esc-1"; e.nombre = "Predica";
    Elemento a; a.id = "el-1"; a.tipo = TipoElemento::Pptx;
    a.ruta = "predica.pptx"; a.modo_pptx = ModoPptx::Directo;
    Elemento b; b.id = "el-2"; b.tipo = TipoElemento::Pptx;
    b.ruta = "otra.pptm"; b.modo_pptx = ModoPptx::Com;
    e.elementos = { a, b };
    p.escenarios.push_back(e);

    const std::string s = AhpFormat::Serializar(p);
    Programa p2;
    std::string err;
    REQUIRE(AhpFormat::CargarFromString(s, &p2, &err));
    REQUIRE(p2.escenarios[0].elementos.size() == 2);
    CHECK(p2.escenarios[0].elementos[0].modo_pptx == ModoPptx::Directo);
    CHECK(p2.escenarios[0].elementos[1].modo_pptx == ModoPptx::Com);
}
