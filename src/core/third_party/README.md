# src/core/third_party/

Cabeceras y amalgamas de terceros. En el cimiento se dejan placeholders
que deben reemplazarse por los archivos reales al preparar el entorno de
build.

## Cómo rellenar esta carpeta (en máquina Windows)

### Opción A: vcpkg

```powershell
vcpkg install spdlog wil nlohmann-json doctest sqlite3
```

Y apuntar `CMAKE_TOOLCHAIN_FILE` a vcpkg en `build.ps1`.

### Opción B: manual (recomendado para reproducibilidad del CI)

1. **sqlite/sqlite3.c** y **sqlite/sqlite3.h**: descargar la amalgama
   desde https://sqlite.org/download.html (versión 3.45+ recomendada)
   y copiarla a `third_party/sqlite/`.

2. **json.hpp**: clonar https://github.com/nlohmann/json y copiar
   `single_include/nlohmann/json.hpp` a `third_party/json.hpp`.

3. **doctest.h**: clonar https://github.com/doctest/doctest y copiar
   `doctest/doctest.h` a `third_party/doctest.h`.

4. **spdlog/**: clonar https://github.com/gabime/spdlog (rama v1.x)
   a `third_party/spdlog/`.

5. **wil/**: clonar https://github.com/microsoft/wil a
   `third_party/wil/`.

## Estado actual (commit fundacional)

Esta carpeta contiene únicamente **placeholders** (`README.md` en cada
subcarpeta). El código fuente referenciado por `CMakeLists.txt` debe
existir físicamente antes de compilar. El CI de GitHub Actions
descargará automáticamente estos archivos en el paso correspondiente
cuando esté implementado (pendiente).

## Licencias

Ver `THIRD_PARTY_LICENSES.txt` en la raíz del repo.
