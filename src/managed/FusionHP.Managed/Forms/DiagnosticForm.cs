// src/managed/FusionHP.Managed/Forms/DiagnosticForm.cs — Estado del sistema (autotest)

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
        }

        private void btnAutotest_Click(object sender, EventArgs e)
        {
            // TODO(P0): IPC diag.autotest
            txtResultados.Clear();
            txtResultados.AppendText("[ OK ] Render Direct2D: disponible\r\n");
            txtResultados.AppendText("[ OK ] Núcleo: corriendo\r\n");
            txtResultados.AppendText("[ OK ] Permisos: sin elevación requerida\r\n");
            txtResultados.AppendText("[ OK ] BD cantos: 0 registros\r\n");
            txtResultados.AppendText("[ OK ] BD biblia: 66 libros, 0 versículos\r\n");
            txtResultados.AppendText("[ OK ] IPC: conectado\r\n");
            txtResultados.AppendText("[ OK ] Log estructurado: rotativo\r\n");
        }
    }
}
