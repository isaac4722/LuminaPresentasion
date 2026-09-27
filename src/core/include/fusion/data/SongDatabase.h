// src/core/include/fusion/data/SongDatabase.h
// cancionero.fdb — base única de cantos. Cargada una vez al arrancar.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace fusion {

struct Canto {
    std::int64_t id;
    std::string  titulo;
    std::string  autor;
    std::string  tono_origen;     // "C", "Am", "F#"
    int          bpm;
    int          veces_usado;
};

struct CantoDetalle : Canto {
    struct Seccion {
        int          orden;
        std::string  tipo;          // verso, coro, puente...
        std::string  etiqueta;      // "Verso 1"
        std::vector<std::string> lineas;
    };
    std::vector<Seccion> secciones;
};

class SongDatabase {
public:
    SongDatabase();
    ~SongDatabase();

    bool Abrir(const std::string& ruta_fdb);  // p.ej. data/cancionero.fdb
    void Cerrar();

    // Lista completa directa (sin tope, sin búsqueda obligatoria).
    std::vector<Canto> ListarTodos() const;

    // Búsqueda ≤200 ms (P2). Sobre vista v_cantos_busqueda.
    std::vector<Canto> Buscar(const std::string& texto, int limite = 100) const;

    // Detalle con secciones.
    bool Obtener(std::int64_t id, CantoDetalle* out) const;

    // Importadores (devuelven número de cantos importados).
    int ImportarHolyricsJson(const std::string& ruta_json);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fusion
