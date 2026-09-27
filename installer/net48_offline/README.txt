# installer/net48_offline/

Aquí debe colocarse el instalador offline oficial de .NET Framework 4.8.

## Descarga

Archivo: `ndp48-x86-x64-offline.exe`
Origen: https://dotnet.microsoft.com/download/dotnet-framework/net48
Tamaño: ~85 MB

## Por qué se necesita

El launcher nativo (`FusionHP.exe`) verifica la presencia de .NET 4.8 en
el sistema. Si no está instalado y el archivo
`net48_offline\ndp48-x86-x64-offline.exe` está presente junto al launcher,
lo arranca automáticamente (una sola vez, con aviso al usuario) y luego
abre el programa.

Si el archivo no está presente, el launcher abre igualmente la versión
"perfil C" (sin .NET) del programa, que es el estudio nativo completo
del núcleo. La carcasa gestionada (perfil A) queda deshabilitada hasta
que se instale .NET.

## .gitignore

Este archivo se excluye del repo por tamaño (~85 MB). Ver `.gitignore`
en la raíz del repo:

```
installer/net48_offline/*.exe
```

## Verificación

El launcher valida el hash SHA-256 del archivo contra un valor
embebido en `src/launcher/main.cpp` (`kNet48OfflineSha256`). Si el hash
no coincide, no lo ejecuta y avisa al usuario.
