// src/core/src/core/ZipDeflate.h — Escritor ZIP con deflate (miniz).
//
// Pieza solicitada y APROBADA en docs/agent/dependencias.md: miniz
// (dominio público) sustituye al escritor almacenado para las
// exportaciones pptx. El OPC es idéntico; las entradas pasan de método 0
// (almacenado) a método 8 (deflate): paquetes mucho más pequeños.
// El propio LectorPptx del núcleo lee deflate y verifica CRC (los tests
// de ida y vuelta siguen siendo la prueba de verdad).

#pragma once

// miniz.h incluye miniz_zip.h (y las definiciones de callbacks que este
// necesita: mz_free_func, mz_realloc_func...). Incluirlo primero.
#include "miniz/miniz.h"

#include <string>

namespace fusion::zipdef {

class EscritorZipDeflate {
public:
    EscritorZipDeflate() {
        ok_iniciado_ = mz_zip_writer_init_heap(&z_, 0, 0) != 0;
    }
    ~EscritorZipDeflate() {
        // mz_zip_writer_end libera incluso tras finalize (no duplica).
        mz_zip_writer_end(&z_);
    }

    EscritorZipDeflate(const EscritorZipDeflate&)            = delete;
    EscritorZipDeflate& operator=(const EscritorZipDeflate&) = delete;

    // Agrega una entrada comprimida (nombres con '/'). Si algo falla
    // queda anotado: Terminar() devolverá "" (nada silencioso).
    void Agregar(const std::string& nombre, const std::string& datos) {
        if (!ok_iniciado_) return;
        if (mz_zip_writer_add_mem(&z_, nombre.c_str(), datos.data(),
                                  datos.size(),
                                  MZ_DEFAULT_LEVEL) == 0) {
            ok_iniciado_ = false;
        }
    }

    // Directorio central + EOCD; paquete completo ("" si algo falló).
    std::string Terminar() {
        if (!ok_iniciado_) return "";
        void* buf = nullptr;
        size_t tam = 0;
        if (mz_zip_writer_finalize_heap_archive(&z_, &buf, &tam) == 0)
            return "";
        std::string out(static_cast<char*>(buf), tam);
        mz_free(buf);
        ok_iniciado_ = false;   // el paquete ya se entregó
        return out;
    }

private:
    mz_zip_archive z_{};
    bool ok_iniciado_ = false;
};

} // namespace fusion::zipdef
