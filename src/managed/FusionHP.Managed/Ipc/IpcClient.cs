// src/managed/FusionHP.Managed/Ipc/IpcClient.cs — Cliente IPC del núcleo
//
// Conecta al pipe con nombre \\.\pipe\FusionHP-ipc y envía/recibe JSON.
// Solo conexiones locales; el núcleo rechaza cualquier conexión externa.

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
        private const string kPipeNombre = @"\\.\pipe\FusionHP-ipc";
        private const int kTimeoutMs = 5000;

        private NamedPipeClientStream _pipe;
        private StreamReader _reader;
        private StreamWriter _writer;
        private readonly object _lock = new object();
        private int _nextMsgId = 1;

        public bool Conectar()
        {
            try
            {
                _pipe = new NamedPipeClientStream(
                    ".", kPipeNombre,
                    PipeDirection.InOut, PipeOptions.Asynchronous);
                _pipe.Connect(kTimeoutMs);
                _reader = new StreamReader(_pipe, new UTF8Encoding(false));
                _writer = new StreamWriter(_pipe, new UTF8Encoding(false))
                          { NewLine = "\n" };
                return _pipe.IsConnected;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Envía un comando y espera la respuesta.
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

                // Esperar respuesta con el mismo id (línea por línea)
                // En el cimiento, lectura bloqueante simple.
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
