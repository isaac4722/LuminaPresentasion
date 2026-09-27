# Convenciones — FUSION-HP

## Lenguaje

- Toda la documentación, mensajes de commit, comentarios del código y logs
  van en **español**.
- Los identificadores de APIs (Win32, .NET, Qt, SQLite) se mantienen en
  inglés porque así se llaman en sus respectivas librerías. No se
  traducen.
- Strings visibles al usuario final: español. La interfaz no será
  bilingüe por defecto.

## C++ (núcleo y launcher)

### Estilo

- C++17.
- `clang-format` con base Google y 4 espacios (ver `.clang-format` en
  `src/core/`).
- Namespaces en `snake_case`. Tipos en `PascalCase`. Funciones en
  `PascalCase`. Variables en `camelCase`. Constantes en `kPascalCase`.
- Miembros privados con sufijo `_` (ej. `monitor_id_`).

### Includes

- Orden: propio proyecto → STL → Windows → Direct2D → terceros.
- Cada header con `#pragma once`.
- Headers del proyecto: `"fusion/..."` con ruta relativa al
  `include/`.

### Errores

- Sin excepciones en el núcleo. Usar `std::expected`-like pattern
  (struct `Resultado<T>` con `ok` + `error`).
- Logs con `spdlog`: `trace`, `debug`, `info`, `warn`, `error`. El nivel
  se configura en `runtime/log.cfg`.
- Nunca `assert` en release.

### Gestión de recursos

- RAII siempre. `std::unique_ptr`, `wil::unique_handle`, `ComPtr`.
- Sin `new`/`delete` explícitos. Sin `malloc`/`free`.
- Sin `goto`.

### Strings

- UTF-8 internamente. Conversión a UTF-16 solo en la frontera con Win32.
- `std::string` para UTF-8, `std::wstring` solo si Win32 lo exige.

## C# (capa gestionada)

### Estilo

- `dotnet format` con `.editorconfig`.
- Namespaces en `PascalCase`. Tipos en `PascalCase`. Métodos en
  `PascalCase`. Variables locales en `camelCase`. Campos privados con
  `_camelCase`.
- `using` solo al principio del archivo, ordenados alfabéticamente.

### Targets duales

- Una sola carpeta de proyecto (`FusionHP.Managed.csproj`).
- `<TargetFrameworks>net35;net48</TargetFrameworks>`.
- Código condicional con `#if NET48` / `#endif` para APIs que solo
  existen en 4.8.
- **Prohibido**: en la capa compartida, cualquier API que no exista en
  .NET 3.5 (LINQ en parte sí está, pero `string.Join` con `IEnumerable`
  no; `Task` no; `async`/`await` no).
- Donde una característica moderna es imprescindible, sacar a un proyecto
  separado que solo compila para `net48` (ej.
  `FusionHP.Managed.OpenXml` para `DocumentFormat.OpenXml`).

### Errores

- Excepciones .NET para errores genuinamente excepcionales (IO roto,
  JSON corrupto). No para control de flujo.
- NLog para logging, mismo formato que el núcleo (`runtime/log.txt`).

## Commits (Convencionales en español)

Formato:

```
tipo(ámbito): descripción en presente, minúscula, sin punto final

- detalle 1
- detalle 2

Ref: #123 (si hay issue)
```

Tipos: `feat`, `fix`, `refactor`, `docs`, `build`, `test`, `chore`,
`perf`.

Ámbitos: `nucleo`, `lanzador`, `gestionada`, `qt`, `ipc`, `datos`,
`importadores`, `installer`, `ci`, `docs`, `calidad`.

### Reglas

- **1 pieza = 1 commit**. Si una pieza toca nucleo + gestionada + ipc,
  va en un mismo commit.
- Máximo 72 caracteres en el asunto.
- Cuerpo en bullets, cada bullet una idea.
- Sin commits "wip" o "fix". Si hace falta, va como `fix(...)`.
- Sin emojis.
- Sin co-authored-by de IA (la autoría del agente se documenta en
  `worklog.md`).

## Worklog

Cada agente (humano o IA) que trabaje en el repo debe leer y escribir en
`worklog.md`. El formato es append-only con secciones `---` separadoras.
Ver `AGENT.md` para el template.

## Nombres de archivos

- C++: `PascalCase.h` y `PascalCase.cpp`. Un header por archivo, un .cpp
  por header.
- C#: un tipo público por archivo, archivo con el nombre del tipo.
- SQL: `snake_case.sql`.
- JSON samples: `snake_case.json`.
- PowerShell: `PascalCase.ps1`.

## Formato JSON

- UTF-8 sin BOM (excepto `.ahp` que permite BOM).
- 2 espacios de indentación.
- Sin trailing commas.
- Strings con comillas dobles.
- Arrays vacíos: `[]`, objetos vacíos: `{}`.
