# data/assets/

Recursos estáticos del producto (fuentes, iconos, fondos, logo).

## Estructura

```
assets/
├── fonts/         Outfit, Cormorant Garamond, Libre Baskerville (OFL)
├── icons/         62 iconos Tabler (4 tintas) en SVG
├── backgrounds/   6 fondos (PNG/JPG, 1920x1080)
└── logo/          Logo propio de FUSION-HP y logo de reposo
```

## Estado actual (commit fundacional)

Las subcarpetas están vacías en este commit. Los assets reales se
descargan al preparar el pack portable final (no en el repo) porque:

- Las fuentes (OFL) son binarios que ocupan ~3 MB.
- Los iconos Tabler (MIT) son binarios que ocupan ~600 KB.
- Los fondos y el logo son binarios de ~6 MB.

Para prepararlos en una máquina de build:

```powershell
# Fuentes (OFL 1.1)
Invoke-WebRequest -Uri "https://fonts.google.com/download?family=Outfit"                -OutFile fonts\Outfit.zip
Invoke-WebRequest -Uri "https://fonts.google.com/download?family=Cormorant%20Garamond"  -OutFile fonts\Cormorant.zip
Invoke-WebRequest -Uri "https://fonts.google.com/download?family=Libre%20Baskerville"   -OutFile fonts\LibreBaskerville.zip

# Iconos Tabler (MIT)
git clone https://github.com/tabler/tabler-icons.git /tmp/tabler
# Copiar los 62 iconos necesarios (lista en docs/agent/icons_list.md, pendiente)

# Logo y fondos
# (Diseño propio — pendiente)
```

## Licencias

| Asset                | Licencia                              |
|----------------------|---------------------------------------|
| Outfit               | SIL Open Font License 1.1             |
| Cormorant Garamond   | SIL Open Font License 1.1             |
| Libre Baskerville    | SIL Open Font License 1.1             |
| Iconos Tabler        | MIT                                   |
| Fondos y logo propios| Propietario (ver LICENSE en la raíz) |

Los textos completos de las licencias deben incluirse en `licenses/` del
pack portable final.
