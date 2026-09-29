// src/core/include/fusion/core/Engine.h
// Motor de proyección: dueño del estado. Sobrevive a cierres de la carcasa.

#pragma once

#include "fusion/Version.h"
#include "fusion/core/Session.h"
#include "fusion/data/BibleDatabase.h"
#include "fusion/data/SongDatabase.h"

#include <cstdint>
#include <memory>
#include <string>
#include <functional>

namespace fusion {

// Snapshot del estado actual del motor. Se devuelve por IPC y se persiste.
struct EstadoMotor {
    std::string programa_ruta;        // path del .ahp cargado (vacío si ninguno)
    std::string programa_titulo;
    std::string escenario_id;
    std::string escenario_nombre;
    std::string elemento_id;
    std::string elemento_titulo;
    int         linea_actual   = 0;
    bool        salida_visible = false;
    bool        negro           = false;
    bool        logo            = false;
    std::string monitor_dispositivo;
    std::string biblia_activa;        // nombre del .fdb abierto (sin extensión)
};

// Tipo de evento emitido por el motor. Los clientes IPC se suscriben.
enum class EventoMotor {
    EstadoCambiado,      // cambió escenario/elemento/línea
    SalidaCambiada,       // visible/oculta/negro/logo
    ProgramaCargado,
    ProgramaCerrado,
    MonitorCambiado,
};

using SuscripcionEventos = std::function<void(EventoMotor, const EstadoMotor&)>;

// Engine es el dueño único del estado de proyección.
// La carcasa (C#/Qt) lo opera exclusivamente por IPC.
class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    // Ciclo de vida ----------------------------------------------------
    bool Iniciar();          // Abre BDs, crea ventana de proyección oculta.
    void Detener();           // Persiste sesión y cierra limpio.

    // Estado -----------------------------------------------------------
    EstadoMotor Estado() const;
    void Suscribir(SuscripcionEventos cb);

    // Programa ---------------------------------------------------------
    bool AbrirPrograma(const std::string& ruta);
    bool NuevoPrograma(const std::string& titulo);
    bool GuardarPrograma(const std::string& ruta /* opcional */);
    bool CerrarPrograma();
    std::vector<std::string> Recientes() const;

    // Programa construible: agregan contenido al programa actual y
    // devuelven los ids generados (para navegar a ellos por IPC).
    bool AgregarTexto(const std::string& titulo,
                      const std::vector<std::string>& lineas,
                      std::string* escenario_out,
                      std::string* elemento_out);
    // Un escenario por canto, un elemento por sección (verso/coro...).
    bool AgregarCanto(std::int64_t canto_id,
                      std::string* escenario_out,
                      std::string* primer_elemento_out);
    // biblia vacía = la activa; modo tercio = lower third.
    bool AgregarVersiculo(const std::string& cita,
                          const std::string& biblia,
                          bool tercio,
                          std::string* escenario_out,
                          std::string* elemento_out);
    // Un elemento por diapositiva (lectura directa, sin PowerPoint).
    bool AgregarPptx(const std::string& ruta,
                     std::string* escenario_out,
                     int* diapositivas_out);
    bool QuitarElemento(const std::string& escenario_id,
                        const std::string& elemento_id);
    // Programa actual serializado ahp.v1 (vacío si no hay programa).
    std::string ProgramaEstado() const;

    // Proyección -------------------------------------------------------
    bool IniciarProyeccion();
    bool DetenerProyeccion();
    // Dibuja el estado actual (elemento/línea, negro, logo) en la ventana
    // de proyección. La pieza que faltaba: el motor CREABA un renderer
    // Direct2D pero NUNCA dibujaba — la proyección era una pantalla negra.
    void Repintar();
    bool IrEscenario(const std::string& escenario_id);
    bool IrElemento(const std::string& escenario_id,
                    const std::string& elemento_id);
    bool IrLinea(const std::string& escenario_id,
                 const std::string& elemento_id,
                 int linea);
    bool Siguiente();
    bool Anterior();
    bool SetNegro(bool activo);
    bool SetLogo(bool activo);
    bool OcultarSalida();

    // Monitor ----------------------------------------------------------
    struct Monitor {
        std::string dispositivo;
        std::string nombre;
        int x, y, w, h;
        bool primario;
    };
    std::vector<Monitor> ListarMonitores() const;
    bool SeleccionarMonitor(const std::string& dispositivo);

    // Biblia -----------------------------------------------------------
    std::vector<std::string> ListarBiblias() const;
    // Selección real de biblia activa (reabre el .fdb). Falso si el
    // nombre no existe en la carpeta de biblias.
    bool SeleccionarBiblia(const std::string& nombre);
    std::string BibliaActiva() const;
    // Árbol completo: 66 libros con nombre/abreviatura y sus capítulos.
    std::vector<LibroBiblia> ListarLibros() const;
    std::vector<int> ListarCapitulos(int libro_id) const;
    // Versículos de un capítulo concreto (para el árbol de la Biblioteca).
    std::vector<Versiculo> ObtenerCapitulo(int libro_id, int capitulo) const;
    bool AgregarFavorito(const std::string& cita);
    bool ObtenerVersiculo(const std::string& biblia,
                          const std::string& cita,
                          std::string* texto_out) const;
    // Búsqueda libre (FTS5) sobre la biblia activa.
    std::vector<Versiculo> BuscarEnBiblia(const std::string& texto,
                                          int limite) const;
    std::vector<std::string> FavoritosBiblia() const;

    // Cantos -----------------------------------------------------------
    // Lista completa con id: la carcasa necesita el id para abrir/agregar.
    std::vector<Canto> ListarCantos() const;
    std::vector<Canto> BuscarCantos(const std::string& texto, int limite) const;
    bool ObtenerCanto(std::int64_t id, CantoDetalle* out) const;

    // Sesión -----------------------------------------------------------
    bool CargarSesion();      // Lee runtime/session.json
    bool GuardarSesion();     // Escribe runtime/session.json

    // Diagnóstico --------------------------------------------------------
    // ¿Hay biblia activa con contenido? (para diag.autotest y avisos)
    bool BibliaLista() const;

private:
    // Actualiza runtime/session.json con la ruta al frente (sin duplicar,
    // tope 10). La llamaban 'recientes' pero NADIE las escribía: la
    // pantalla de inicio siempre mostraba la lista vacía.
    void RegistrarReciente(const std::string& ruta);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fusion
