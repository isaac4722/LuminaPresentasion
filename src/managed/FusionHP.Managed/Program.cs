// src/managed/FusionHP.Managed/Program.cs — Punto de entrada de la carcasa gestionada
//
// La carcasa NO es dueña del estado de proyección. Solo opera el núcleo
// por IPC (\\.\pipe\FusionHP-ipc). Si la carcasa se cierra, el núcleo
// sigue proyectando.
//
// La conexión es PACIENTE y VISIBLE: una ventana de espera con progreso
// y botón Cancelar (primer arranque: el núcleo puede estar sembrando la
// biblia unos segundos). El error con detalle solo aparece si el usuario
// cancela o la conexión no llega a término.

using System;
using System.Windows.Forms;
using FusionHP.Managed.Forms;

namespace FusionHP.Managed
{
    internal static class Program
    {
        [STAThread]
        private static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            using (var ipc = new Ipc.IpcClient())
            {
                bool conectado;
                using (var espera = new EsperaConexionForm(ipc))
                {
                    espera.ShowDialog();
                    conectado = espera.Conectado;

                    if (!conectado && espera.Cancelado)
                    {
                        // El operador no quiso seguir esperando: salida limpia.
                        return;
                    }
                }

                if (!conectado)
                {
                    MessageBox.Show(
                        "No se pudo conectar al núcleo de FUSION-HP " +
                        "(FusionCore.exe).\n\n" +
                        ipc.UltimoError + "\n\n" +
                        "Si el problema persiste, revisa runtime\\arranque.log " +
                        "y runtime\\nucleo.log junto al programa y compártelo " +
                        "con el reporte.",
                        "FUSION-HP", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                var main = new MainForm(ipc);
                Application.Run(main);
            }
        }
    }
}
