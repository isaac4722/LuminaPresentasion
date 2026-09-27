# Protocolo `ipc.v1` — IPC interno de FUSION-HP

> **Versión**: `ipc.v1`
> **Transporte**: Pipe con nombre `\\.\pipe\FusionHP-ipc`
> **Codificación**: JSON UTF-8 sin BOM, mensajes separados por `\n`
> **Solo conexiones locales**: el servidor rechaza cualquier cliente que no
> venga del mismo SID de usuario.

## Roles

- **Servidor**: el núcleo nativo (`FusionCore.exe`). Escucha el pipe.
- **Clientes**: la carcasa gestionada (`FusionHP.Managed.exe`), la carcasa Qt
  (`FusionQtShell.exe`) y (en modo operador) una segunda conexión desde la
  misma carcasa gestionada.

## Mensaje base

Todo mensaje es un objeto JSON con esta forma:

```json
{
  "ipc": "fusion",
  "version": 1,
  "type": "comando",
  "id": "msg-001",
  "payload": { ... }
}
```

- `ipc`: siempre `"fusion"`. Permite detectar conexiones equivocadas.
- `version`: entero, actualmente `1`.
- `type`: nombre del comando (ver tabla abajo).
- `id`: string, identificador único del mensaje (UUID o contador del
  cliente). El servidor lo repite en la respuesta.
- `payload`: objeto con los datos del comando.

## Respuestas

El servidor responde siempre, aunque sea con error:

```json
{
  "ipc": "fusion",
  "version": 1,
  "id": "msg-001",
  "ok": true,
  "result": { ... }
}
```

o

```json
{
  "ipc": "fusion",
  "version": 1,
  "id": "msg-001",
  "ok": false,
  "error": {
    "code": "E_NO_PROGRAM",
    "message": "No hay programa cargado"
  }
}
```

## Eventos

El servidor puede enviar mensajes **sin `id` correlativo a una petición**;
estos son eventos que el cliente debe manejar (suscripción). Tienen
`type` empezando por `evento.`:

```json
{
  "ipc": "fusion",
  "version": 1,
  "type": "evento.estado_cambiado",
  "payload": {
    "programa": "culto-dom.ahp",
    "escenario": "esc-001",
    "elemento": "el-002",
    "linea": 1,
    "salida_visible": true
  }
}
```

## Comandos

### Estado y sesión

| Comando             | Dirección  | Payload                              | Result                          |
|---------------------|------------|--------------------------------------|---------------------------------|
| `estado.lector`     | C→S        | `{}`                                 | Snapshot de `session.json`     |
| `estado.suscribir`  | C→S        | `{ eventos: ["estado_cambiado"] }`   | `{ ok: true }`                  |
| `session.guardar`   | C→S        | `{ programa, escenario, elemento }`   | `{ ok: true }`                  |

### Programa

| Comando             | Payload                                  | Result                          |
|---------------------|------------------------------------------|---------------------------------|
| `programa.abrir`    | `{ ruta: "data/programas/culto.ahp" }`   | `{ ok, programa: {...} }`       |
| `programa.nuevo`    | `{ meta: {...} }`                        | `{ ok, programa: {...} }`       |
| `programa.guardar`  | `{ ruta? }`                              | `{ ok, ruta }`                  |
| `programa.cerrar`    | `{}`                                     | `{ ok }`                        |
| `programa.recientes`| `{}`                                     | `{ ok, recientes: [...] }`      |

### Proyección

| Comando               | Payload                                       | Result                          |
|-----------------------|-----------------------------------------------|---------------------------------|
| `proyeccion.iniciar`  | `{}`                                          | `{ ok, monitor }`               |
| `proyeccion.detener`  | `{}`                                          | `{ ok }`                        |
| `proyeccion.escenario`| `{ escenario_id }`                            | `{ ok }`                        |
| `proyeccion.elemento` | `{ escenario_id, elemento_id }`               | `{ ok }`                        |
| `proyeccion.linea`    | `{ escenario_id, elemento_id, linea }`       | `{ ok }`                        |
| `proyeccion.siguiente`| `{}`                                          | `{ ok, elemento_id }`           |
| `proyeccion.anterior` | `{}`                                          | `{ ok, elemento_id }`           |
| `proyeccion.negro`    | `{ activar: bool }`                           | `{ ok }`                        |
| `proyeccion.logo`     | `{ activar: bool }`                           | `{ ok }`                        |
| `proyeccion.ocultar`  | `{}`                                          | `{ ok }`                        |

### Monitor

| Comando            | Payload                                  | Result                          |
|--------------------|------------------------------------------|---------------------------------|
| `monitor.listar`   | `{}`                                     | `{ ok, monitores: [...] }`      |
| `monitor.seleccionar` | `{ dispositivo: "..." }`             | `{ ok }`                        |

### Biblia

| Comando             | Payload                                  | Result                          |
|---------------------|------------------------------------------|---------------------------------|
| `biblia.listar`     | `{}`                                     | `{ ok, biblias: [...] }`        |
| `biblia.obtener`    | `{ biblia, cita }`                       | `{ ok, texto }`                 |
| `biblia.buscar`     | `{ biblia, texto, limite? }`             | `{ ok, resultados: [...] }`      |
| `biblia.favoritos`  | `{}`                                     | `{ ok, favoritos: [...] }`      |

### Cantos

| Comando             | Payload                                  | Result                          |
|---------------------|------------------------------------------|---------------------------------|
| `canto.listar`      | `{}`                                     | `{ ok, cantos: [...] }`         |
| `canto.obtener`     | `{ id }`                                 | `{ ok, canto: {...} }`          |
| `canto.buscar`      | `{ texto }`                              | `{ ok, resultados: [...] }`      |

### Temas

| Comando               | Payload                                  | Result                          |
|-----------------------|------------------------------------------|---------------------------------|
| `tema.listar`         | `{}`                                     | `{ ok, temas: [...] }`          |
| `tema.aplicar_elemento` | `{ elemento_id, tema }`               | `{ ok, informe_fidelidad }`     |
| `tema.aplicar_todo`   | `{ tema }`                               | `{ ok, informe_fidelidad }`      |

### Diagnóstico

| Comando            | Payload | Result                          |
|--------------------|---------|---------------------------------|
| `diag.autotest`    | `{}`    | `{ ok, resultados: [...] }`      |
| `diag.log_tail`    | `{ n }` | `{ ok, lineas: [...] }`          |

## Timeouts

- Operación normal: 5 segundos.
- Carga de biblia grande o autotest: 30 segundos.
- Si un cliente excede, el servidor responde con error `E_TIMEOUT`.

## Códigos de error

| Código              | Significado                                     |
|---------------------|-------------------------------------------------|
| `E_BAD_PAYLOAD`     | Payload malformado o falta de campos            |
| `E_NO_PROGRAM`      | No hay programa cargado                         |
| `E_NOT_FOUND`       | Recurso no encontrado (escenario, elemento...)   |
| `E_IO`              | Error de E/S (archivo, BD)                      |
| `E_TIMEOUT`         | Operación excedió el timeout                    |
| `E_INTERNAL`        | Error interno del núcleo (ver log)              |
| `E_UNSUPPORTED`     | Mensaje no soportado en esta versión del núcleo |

## Seguridad

- El pipe se crea con DACL que solo permite conexiones del mismo SID de
  usuario. Otras cuentas (incluida SYSTEM) no pueden conectarse.
- No hay autenticación criptográfica: el aislamiento por SID basta porque
  el transporte es local.
- Cualquier intento de conexión desde otra máquina es rechazado a nivel
  de pipe (no es un pipe de red).

## Versionado

- Cambios compatibles hacia adelante (añadir campos al payload): no
  requieren subir versión. El cliente los ignora si no los entiende.
- Cambios incompatibles (renombrar/eliminar campos, cambiar semántica):
  subir a `ipc.v2` y mantener retrocompatibilidad al menos una release.
