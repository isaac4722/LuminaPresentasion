// src/core/src/importers/TransaccionImport.h — Guardia RAII de importación
//
// Los importadores masivos (biblias y cantos) envuelven su bucle de
// inserción en UNA transacción. Sin ella, cada INSERT era una transacción
// implícita con vuelco a disco: sembrar la RVR1909 (~31.000 versículos)
// tardaba minutos y el núcleo no creaba el pipe IPC hasta terminar —
// la consola se rendía a los ~12 s con "No se pudo conectar al núcleo".
//
// RAII: cualquier return/salida temprana del importador hace ROLLBACK
// (nada de importaciones a medias silenciosas); la confirmación es
// explícita con Confirma().

#pragma once

namespace fusion::importadores {

template <typename Bd>
class TransaccionImport {
public:
    explicit TransaccionImport(Bd& bd) : bd_(bd), activa_(bd.IniciarTransaccion()) {}
    ~TransaccionImport() { if (activa_) bd_.DescartarTransaccion(); }

    TransaccionImport(const TransaccionImport&)            = delete;
    TransaccionImport& operator=(const TransaccionImport&) = delete;

    bool Activa() const { return activa_; }

    // Confirma la transacción. Devuelve false si COMMIT falló (la
    // destrucción ya no reintenta: la transacción quedó cerrada por
    // sqlite o sigue abierta y el ROLLBACK final la limpiará).
    bool Confirma() {
        if (!activa_) return false;
        activa_ = false;
        return bd_.ConfirmarTransaccion();
    }

private:
    Bd&  bd_;
    bool activa_;
};

} // namespace fusion::importadores
