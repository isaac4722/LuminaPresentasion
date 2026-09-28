// src/managed/FusionHP.Managed/Program.cs — Punto de entrada de la carcasa gestionada
//
// La carcasa NO es dueña del estado de proyección. Solo opera el núcleo
// por IPC (\\.\pipe\FusionHP-ipc). Si la carcasa se cierra, el núcleo
// sigue proyectando.

using System;
using System.Windows.Forms;

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
                if (!ipc.Conectar())
                {
                    MessageBox.Show(
                        "No se pudo conectar al núcleo de FUSION-HP (FusionCore.exe).\n\n" +
                        ipc.UltimoError + "\n\n" +
                        "Si el problema persiste, revisa runtime\\arranque.log " +
                        "junto al programa y compártelo con el reporte.",
                        "FUSION-HP", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                var main = new Forms.MainForm(ipc);
                Application.Run(main);
            }
        }
    }
}
