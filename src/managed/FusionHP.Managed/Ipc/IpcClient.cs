// src/managed/FusionHP.Managed/Ipc/IpcClient.cs — Cliente IPC del núcleo
//
// Conecta al pipe con nombre \\.\pipe\FusionHP-ipc y envía/recibe JSON.
// Solo conexiones locales; el núcleo rechaza cualquier conexión externa.
//
// OJO (causa del "no abre"): NamedPipeClientStream espera el NOMBRE del
// pipe (p. ej. "FusionHP-ipc") y construye la ruta \\.\pipe\<nombre> por
// su cuenta. Pasarle la ruta completa producía la ruta malformada
// \\.\pipe\\\.\pipe\FusionHP-ipc y la conexión fallaba SIEMPRE: la
// consola mostraba "No se pudo conectar al núcleo (FusionCore.exe)".

using System;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Threading;

namespace FusionHP.Managed.Ipc
{
    /// <summary>
    /// Cliente IPC hacia el núcleo FUSION-HP. Una sola conexión por proceso cliente.
    /// </summary>
    public sealed class IpcClient : IDisposable
    {
        // Nombre del pipe SIN el prefijo \\.\pipe\ (lo añade .NET).
        public const string NombrePipe = "FusionHP-ipc";
        private const int kTimeoutMs = 3000;
        private const int kIntentos = 3;

        private NamedPipeClientStream _pipe;
        private StreamReader _reader;
        private StreamWriter _writer;
        private readonly object _lock = new object();
        private int _nextMsgId = 1;

        /// <summary>Detalle del último fallo de conexión (para diagnóstico).</summary>
        public string UltimoError { get; private set; }

        public bool Conectar()
        {
            UltimoError = null;
            // Reintentos cortos: el launcher arranca núcleo y consola casi
            // a la vez; el servidor IPC tarda milisegundos, pero no hay que
            // rendirse al primer intento durante el arranque.
            for (int intento = 1; intento <= kIntentos; ++intento)
            {
                try
                {
                    _pipe = new NamedPipeClientStream(
                        ".", NombrePipe,
                        PipeDirection.InOut, PipeOptions.Asynchronous);
                    _pipe.Connect(kTimeoutMs);
                    _reader = new StreamReader(_pipe, new UTF8Encoding(false));
                    _writer = new StreamWriter(_pipe, new UTF8Encoding(false))
                              { NewLine = "\n" };
                    if (_pipe.IsConnected) return true;
                    UltimoError = "El pipe quedó sin conectar tras Connect().";
                }
                catch (Exception ex)
                {
                    UltimoError = ex.Message;
                }

                Dispose();
                if (intento < kIntentos) Thread.Sleep(1500);
            }
            UltimoError = "No se pudo abrir el pipe \\\\.\\pipe\\" + NombrePipe +
                          " tras " + kIntentos + " intentos. Último error: " +
                          (UltimoError ?? "desconocido") +
                          ". Revisa runtime\\arranque.log junto al programa.";
            return false;
        }

        /// <summary>
        /// Envía un comando y espera la respuesta (con el mismo id).
        /// </summary>
        public string Enviar(string tipo, string payloadJson)
        {
            lock (_lock)
            {
                if (_writer == null) return null;
                string id = "msg-" + (_nextMsgId++);
                string msg = "{" +
                    "\"ipc\":\"fusion\"," +
                    "\"version\":1," +
                    "\"type\":\"" + tipo + "\"," +
                    "\"id\":\"" + id + "\"," +
                    "\"payload\":" + (payloadJson ?? "null") +
                    "}";
                _writer.WriteLine(msg);
                _writer.Flush();

                // Esperar respuesta con el mismo id (línea por línea).
                // TODO(P0): async + cancelación por timeout.
                var sw = System.Diagnostics.Stopwatch.StartNew();
                while (sw.ElapsedMilliseconds < kTimeoutMs)
                {
                    string line = _reader.ReadLine();
                    if (line == null) return null;
                    if (line.Contains("\"id\":\"" + id + "\"")) return line;
                }
                return null;
            }
        }

        public void Dispose()
        {
            lock (_lock)
            {
                if (_writer != null) { _writer.Dispose(); _writer = null; }
                if (_reader != null) { _reader.Dispose(); _reader = null; }
                if (_pipe != null)   { _pipe.Dispose();   _pipe = null; }
            }
        }
    }
}
