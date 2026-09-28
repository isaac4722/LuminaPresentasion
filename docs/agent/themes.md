# Temas y herencia (5 niveles)

Esquema de un tema y algoritmo de resolución. Complementa
`format_ahp_v1.md`, sección "Temas y herencia".

## 1. Qué es un tema

Un tema es un **bolso de propiedades planas** (`clave → valor` en texto).
Nada de objetos anidados: las claves usan notación de punto y cada valor es
un string. Esto permite que un nivel sobreescriba **selectivamente** una
propiedad sin tener que repetir el resto.

Claves reconocidas por el renderizador:

| Clave                    | Valores                              | Ejemplo      |
|--------------------------|--------------------------------------|--------------|
| `texto.familia`          | nombre de fuente (UTF-8)             | `Outfit`     |
| `texto.tamano`           | puntos, 1–2000                       | `60`         |
| `texto.color`            | `#RRGGBB` o `#RGB`                   | `#FFFFFF`    |
| `texto.negrita`          | `true` / `false`                     | `true`       |
| `texto.cursiva`          | `true` / `false`                     | `false`      |
| `texto.alineacion_centro`| `true` / `false`                     | `true`       |
| `fondo.tipo`             | `solido` / `gradiente` / `imagen`    | `gradiente`  |
| `fondo.color1`           | `#RRGGBB`                            | `#0B1F3A`    |
| `fondo.color2`           | `#RRGGBB` (segundo color del gradiente) | `#000000` |
| `fondo.ruta_imagen`      | ruta relativa al ejecutable          | `fondo.png`  |
| `fondo.ajuste`           | `cubrir` / `contener` / `estirar`    | `cubrir`     |

Las claves desconocidas se **conservan** (tolerancia hacia adelante) y
también aparecen en el informe de fidelidad si cambian; el renderizador
simplemente las ignora.

## 2. Biblioteca de temas (`runtime/temas.json`)

```json
{
  "formato": "temas",
  "version": 1,
  "temas": {
    "DominicalCálido": {
      "texto.familia": "Outfit",
      "texto.color": "#FFFFFF",
      "fondo.tipo": "gradiente",
      "fondo.color1": "#0B1F3A",
      "fondo.color2": "#000000"
    },
    "Sobrio": { "texto.color": "#EEEEEE" }
  }
}
```

Reglas de validación (parsing estricto):

1. `version > 1` → error explícito, no silencioso (igual que `ahp.v1`).
2. Cada tema debe ser un objeto; cada propiedad, un string.
3. Un tema puede definir un subconjunto de claves (tema parcial).

## 3. Los 5 niveles

Cascada del doc técnico 5.4 (Tema → Plantilla de Escenario → Escenario →
Elemento) más el nivel runtime como extensión propia (desviación
aceptada: el "en caliente" estilo Holyrics [HOLY] es un requisito P0
del producto; el doc 5.4 lo cubre con el motor en vivo, aquí además
queda persistido en `session.json`).

| Nivel | Nombre       | Origen                                          | Referencia                          |
|-------|--------------|--------------------------------------------------|-------------------------------------|
| 1     | Raíz         | Tema raíz del programa                           | `meta.tema_raiz` (ahp.v1)           |
| 2     | Plantilla    | Plantilla del escenario                          | `escenario.tema` (ahp.v1)           |
| 3     | Escenario    | Capa inline del escenario (fondo + tema_escenario) | `escenario.fondo` + `escenario.tema_escenario` (ahp.v1) |
| 4     | Elemento     | Tema override del elemento                       | `elemento.tema_override` (ahp.v1)   |
| 5     | Runtime      | Tema runtime del operador                        | `Sesion.tema_runtime` (session.json) |

Los niveles 1, 2 y 4 se resuelven **por nombre** contra la biblioteca de
temas. El nivel 3 es un bolso inline construido con
`CapaEscenario(fondo, tema_escenario)`: el color sólido `fondo` se pliega
como `fondo.tipo=solido` + `fondo.color1` (destino del "fondo del diseño"
al importar PPTX, doc 9.2.6) y las claves del bolso `tema_escenario`
ganan sobre el plegado (lo explícito manda; ambas fuentes viven en el
MISMO nivel, así que pisa el plegado sin registrar entradas extra en el
informe). El nivel 5 es un bolso de propiedades que el operador aplica
en caliente (cambiar color de fondo "ahora", subir tamaño, etc.), no un
tema nombrado.

El nivel Escenario es lo que el operador fija para TODO el escenario por
encima de su plantilla: es el tercer eslabón de la cascada del doc 5.4
y el equivalente del nivel "Diseño" de PresentationML en la traducción
del exportador PPTX (9.3).

## 4. Algoritmo de resolución

`fusion::core::HerenciaTemas::Resolver(raiz, plantilla, escenario, elemento, runtime)`:

1. Empezar con un bolso vacío.
2. Recorrer los niveles en orden 1 → 5 (prioridad creciente).
3. Por cada propiedad de la capa:
   - Si la clave **no existía**: se añade y entra al informe con
     `valor_anterior = ""`.
   - Si la clave existía y el valor **es distinto**: se sobreescribe y
     entra al informe con el valor anterior.
   - Si la clave existía con el **mismo valor**: no entra al informe
     (decisión: el informe enumera cambios efectivos; reafirmar el mismo
     valor no es una sobreescritura que el operador necesite revisar).
4. Una capa nula (nivel sin tema) se ignora.

El resultado es `ResolucionTema { estilo_resuelto, informe }`. El
**informe de fidelidad** queda en orden de aplicación, con nivel, propiedad,
valor anterior y valor nuevo, de modo que el operador pueda auditar por qué
un elemento se ve como se ve.

## 5. Aplicación al renderizador

`AplicarAEstilos(resuelto, &texto, &fondo)` convierte el bolso resuelto a
`EstiloTexto` + `Fondo` (ver `Renderer.h`). Políticas:

- Valor inválido (hex mal formado, tipo desconocido, tamaño fuera de
  rango): se **ignora esa clave** y el estilo conserva su valor anterior;
  la función devuelve `false` para que el caller pueda registrar un
  `warning`. Nunca aborta la proyección.
- Claves ajenas a texto/fondo: se conservan sin efecto.
- `texto.familia` y `fondo.ruta_imagen` aceptan UTF-8 (se convierte a
  UTF-16 con decodificador propio, portable).

## 6. Tests

- Nativos: `tests/native/test_herencia.cpp`
  (prioridad 5 niveles, informe, plegado de CapaEscenario, valores
  inválidos, biblioteca, integración ahp.v1).
- La resolución es portable (sin API de Windows) para poder correr los
  tests en cualquier plataforma además de MSVC.
