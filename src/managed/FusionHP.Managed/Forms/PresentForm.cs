// src/managed/FusionHP.Managed/Forms/PresentForm.cs — Presentar (proyección)

using System;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class PresentForm : Form
    {
        private readonly IpcClient _ipc;

        public PresentForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            CargarMonitores();
        }

        private void CargarMonitores()
        {
            // TODO(P0): IPC monitor.listar
            cmbMonitores.Items.Clear();
            cmbMonitores.Items.Add(@"\\.\DISPLAY1 (Primario)");
            cmbMonitores.Items.Add(@"\\.\DISPLAY2 (Proyector)");
            if (cmbMonitores.Items.Count > 1) cmbMonitores.SelectedIndex = 1;
        }

        // Atajos Holyrics: flechas, Espacio, Esc, B/C/L, G, F5, F8
        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            switch (keyData)
            {
                case Keys.Escape: _ipc.Enviar("proyeccion.detener", "{}"); Close(); return true;
                case Keys.Space:
                case Keys.Right:
                case Keys.Down:    _ipc.Enviar("proyeccion.siguiente", "{}"); return true;
                case Keys.Left:
                case Keys.Up:      _ipc.Enviar("proyeccion.anterior", "{}"); return true;
                case Keys.B:       _ipc.Enviar("proyeccion.negro", "{\"activar\":true}"); return true;
                case Keys.C:       _ipc.Enviar("proyeccion.logo",  "{\"activar\":false}"); return true;
                case Keys.L:       _ipc.Enviar("proyeccion.logo",  "{\"activar\":true}"); return true;
                case Keys.G:       using (var f = new BibleQuickForm(_ipc)) f.ShowDialog(this); return true;
                case Keys.F5:      _ipc.Enviar("proyeccion.iniciar", "{}"); return true;
                case Keys.F8:      using (var f = new OperatorConsoleForm(_ipc)) f.ShowDialog(this); return true;
            }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        private void btnIniciar_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.iniciar", "{}");
        }

        private void btnDetener_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.detener", "{}");
        }

        private void btnNegro_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.negro", "{\"activar\":true}");
        }

        private void btnLogo_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.logo", "{\"activar\":true}");
        }
    }
}
