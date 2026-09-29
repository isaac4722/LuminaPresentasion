// src/managed/FusionHP.Managed/Forms/DiagnosticForm.cs — Estado del sistema REAL
//
// Antes: un autotest FINGIDO ("[ OK ] ...") pintado a mano. Ahora corre
// el autotest del núcleo (diag.autotest) y añade el estado real y los
// conteos de las bases de datos.

using System;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class DiagnosticForm : Form
    {
        private readonly IpcClient _ipc;

        public DiagnosticForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            EjecutarAutotest();
        }

        private void btnAutotest_Click(object sender, EventArgs e)
        {
            EjecutarAutotest();
        }

        private void EjecutarAutotest()
        {
            txtResultados.Clear();

            // Estado del motor.
            var e = _ipc.Pedir("estado.lector", "{}");
            if (e != null && IpcJson.Ok(e))
            {
                txtResultados.AppendText(
                    "[ INFO ] Núcleo: respondiendo ipc.v1\r\n");
                txtResultados.AppendText(
                    "[ INFO ] Programa: " +
                    (IpcJson.Texto(e, "programa_titulo", "").Length > 0
                         ? IpcJson.Texto(e, "programa_titulo")
                         : "(ninguno)") + "\r\n");
                txtResultados.AppendText(
                    "[ INFO ] Biblia activa: " +
                    (IpcJson.Texto(e, "biblia_activa", "").Length > 0
                         ? IpcJson.Texto(e, "biblia_activa")
                         : "(ninguna)") + "\r\n");
            }
            else
            {
                txtResultados.AppendText(
                    "[ FALLO ] Núcleo: sin respuesta (" +
                    IpcJson.MensajeError(e) + ")\r\n");
            }

            // Autotest del núcleo (biblia, cancionero, monitores...).
            var r = _ipc.Pedir("diag.autotest", "{}");
            if (r != null && IpcJson.Ok(r))
            {
                var arr = IpcJson.Arreglo(r, "resultados");
                if (arr != null)
                {
                    foreach (var item in arr)
                    {
                        var p = IpcJson.ObjetoEn(item);
                        if (p == null) continue;
                        bool ok = IpcJson.Booleano(p, "ok");
                        txtResultados.AppendText(
                            (ok ? "[ OK   ] " : "[ FALLO] ") +
                            IpcJson.Texto(p, "nombre") + ": " +
                            IpcJson.Texto(p, "detalle") + "\r\n");
                    }
                }
            }
            else
            {
                txtResultados.AppendText(
                    "[ FALLO ] diag.autotest: " +
                    IpcJson.MensajeError(r) + "\r\n");
            }

            // Bitácora reciente del núcleo (observabilidad).
            var rl = _ipc.Pedir("diag.log_tail", "{\"n\":8}");
            if (rl != null && IpcJson.Ok(rl))
            {
                txtResultados.AppendText("\r\n— Bitácora del núcleo —\r\n");
                var lineas = IpcJson.Arreglo(rl, "lineas");
                if (lineas != null)
                {
                    foreach (var item in lineas)
                        txtResultados.AppendText("  " +
                            Convert.ToString(item) + "\r\n");
                }
            }
        }
    }
}
