// src/core/include/fusion/core/Rutas.h — Raíces de datos escribibles
//
// Regla del producto: sin Registro, 100% local. Los datos MUTABLES del
// núcleo (cancionero.fdb, biblias *.fdb, sesión, bitácora) deben vivir
// en una carpeta ESCRIBIBLE:
//   - Portable (zip descomprimido en el escritorio): junto al ejecutable.
//   - Instalado en C:\Program Files (no escribible sin elevación):
//     %LOCALAPPDATA%\FUSION-HP. Sin esto, sqlite3_open no podía crear
//     cancionero.fdb y el núcleo arrancaba con las BD muertas.
// Los assets de SOLO LECTURA (esquemas, semilla RVR1909.json) siempre
// se leen de la carpeta de instalación.

#pragma once

#include <string>

namespace fusion {
namespace rutas {

// Carpeta del ejecutable (sin barra final).
std::wstring RaizInstalacion();

// ¿Se puede escribir en esa carpeta? (prueba con archivo temporal)
bool CarpetaEscribible(const std::wstring& dir);

// Raíz de datos escribibles (cacheada al primer uso):
//   carpeta del exe si es escribible; si no, %LOCALAPPDATA%\FUSION-HP
//   (creando data\bibles y runtime). Devuelve ruta ancha.
const std::wstring& RaizDatosW();

// Rutas concretas (UTF-8, listas para sqlite3_open):
std::string CancioneroFdb();      // ...\data\cancionero.fdb
std::string CarpetaBiblias();     // ...\data\bibles
std::string BibliaFdb(const std::string& nombre_fdb);  // ...\data\bibles\<n>.fdb
std::string SesionJson();         // ...\runtime\session.json
std::string BitacoraNucleo();     // ...\runtime\nucleo.log

// Variante ancha de carpeta de biblias (para barrer con FindFirstFileW).
std::wstring CarpetaBibliasW();

// Semilla de RVR1909 (asset de solo lectura junto al ejecutable):
//   <exe>\data\bibles\RVR1909.json   (vacío si no existe)
std::string SemillaRVR1909Json();
bool ExisteArchivo(const std::wstring& ruta);

// Bitácora del núcleo: añade la línea con sello de tiempo UTF-8 a
// runtime\nucleo.log en la raíz de datos. Nunca lanza.
void Bitacora(const std::string& linea);

} // namespace rutas
} // namespace fusion
