// src/core/include/fusion/data/SemillaCantos.h — Cantos de siembra
//
// El cancionero nacía VACÍO: la Biblioteca mostraba una lista vacía y la
// app parecía rota aunque todo funcionara. Esta pieza siembra himnos
// clásicos de dominio público al primer arranque (solo si la tabla está
// vacía), para que la app sea usable desde el minuto uno. El operador
// puede borrarlos o importar los suyos (Holyrics JSON, manual).
//
// PORTABLE: puro dato + SongDatabase::InsertarCanto. Se prueba en el
// arnés local (gcc) igual que en MSVC.

#pragma once

#include "fusion/data/SongDatabase.h"

#include <vector>

namespace fusion {
namespace semillacantos {

// Himnos clásicos (dominio público) con secciones y líneas reales.
// Nunca vacío; cada canto con al menos una sección con líneas.
std::vector<CantoDetalle> Generar();

} // namespace semillacantos
} // namespace fusion
