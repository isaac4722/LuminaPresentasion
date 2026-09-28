// src/managed/FusionHP.Managed.Tests/IpcClientTests.cs
// Regresión del "no abre Core": el nombre del pipe NUNCA debe llevar el
// prefijo \\.\pipe\ — NamedPipeClientStream lo añade por su cuenta, y
// pasárselo producía la ruta malformada \\.\pipe\\\.\pipe\FusionHP-ipc
// (la consola mostraba "No se pudo conectar al núcleo" siempre).

using System;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Tests
{
    public class IpcClientTests
    {
        [Test]
        public void NombrePipe_SinPrefijo()
        {
            string nombre = IpcClient.NombrePipe;
            if (string.IsNullOrEmpty(nombre))
                throw new Exception("El nombre del pipe está vacío");
            if (nombre.Contains("\\\\"))
                throw new Exception("El nombre del pipe no debe contener '\\': " +
                                    "NamedPipeClientStream añade \\\\.\\pipe\\ por su cuenta " +
                                    "(causa del \"no abre Core\")");
            if (nombre.StartsWith("\\\\.\\pipe\\", StringComparison.OrdinalIgnoreCase))
                throw new Exception("El nombre del pipe lleva el prefijo \\\\.\\pipe\\");
            if (nombre != "FusionHP-ipc")
                throw new Exception("El nombre del pipe debe ser exactamente " +
                                    "'FusionHP-ipc' (el núcleo crea ese nombre): " + nombre);
        }

        [Test]
        public void Enviar_SinConexion_DevuelveNullSinExcepcion()
        {
            using (var ipc = new IpcClient())
            {
                string r = ipc.Enviar("estado.lector", "{}");
                if (r != null)
                    throw new Exception("Enviar sin conexión debía devolver null");
            }
        }

        [Test]
        public void Conectar_SinNucleo_FallaConDetalle()
        {
            // Sin FusionCore.exe corriendo, Conectar debe fallar de forma
            // controlada (false + UltimoError con detalle), nunca colgarse
            // ni lanzar. 3 intentos × 3 s + pausas: acotado a ~12 s.
            using (var ipc = new IpcClient())
            {
                bool ok = ipc.Conectar();
                // Si un núcleo real está escuchando (p. ej. en la máquina del
                // desarrollador), la conexión es legítima y no hay nada que
                // validar más allá de la coherencia del resultado.
                if (!ok && string.IsNullOrEmpty(ipc.UltimoError))
                    throw new Exception("Conectar falló sin dejar UltimoError");
            }
        }
    }
}
