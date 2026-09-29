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

        [Test]
        public void ConectarPaciente_CanceladoAlInicio_SaleSinColgarse()
        {
            // La conexión paciente del arranque (ventana "Conectando…")
            // debe responder a la cancelación inmediata: false + detalle,
            // sin excepción y en tiempo acotado (el primer intento de
            // Connect(2000) puede llegar a esperar 2 s como mucho; con el
            // check previo, lo normal es salir casi al instante).
            using (var ipc = new IpcClient())
            {
                bool ok = ipc.ConectarPaciente(() => true);
                if (ok)
                    return;   // había un núcleo real: conexión legítima
                if (string.IsNullOrEmpty(ipc.UltimoError))
                    throw new Exception("ConectarPaciente falló sin " +
                                        "UltimoError");
            }
        }

        [Test]
        public void ConectarPaciente_CancelacionTardia_SeRespetaEntreIntentos()
        {
            // Contador: cancela a partir del segundo sondeo de pausa. Esto
            // recorre el camino "intento falló → pausa troceada → cancel".
            using (var ipc = new IpcClient())
            {
                int llamadas = 0;
                bool ok = ipc.ConectarPaciente(() => ++llamadas > 8);
                if (ok) return;   // núcleo real presente
                if (llamadas < 8)
                    throw new Exception("La cancelación no se consultó: " +
                                        llamadas + " llamadas");
            }
        }
    }
}
