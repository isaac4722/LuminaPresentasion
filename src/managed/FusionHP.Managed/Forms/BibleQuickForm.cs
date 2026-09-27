// src/managed/FusionHP.Managed/Forms/BibleQuickForm.cs — Biblia rápida (G)

using System;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class BibleQuickForm : Form
    {
        private readonly IpcClient _ipc;

        public BibleQuickForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
        }

        private void btnIr_Click(object sender, EventArgs e)
        {
            string cita = txtCita.Text.Trim();
            if (string.IsNullOrEmpty(cita)) return;
            // TODO(P0): IPC biblia.obtener { biblia, cita }
            txtResultado.Text = "(consultando al núcleo...)";
        }

        private void btnFavorito_Click(object sender, EventArgs e)
        {
            string cita = txtCita.Text.Trim();
            if (string.IsNullOrEmpty(cita)) return;
            // TODO(P0): IPC biblia.favoritos.agregar { cita }
        }

        private void btnTercio_Click(object sender, EventArgs e)
        {
            // Modo Tercio: proyecta el versículo como lower third.
            _ipc.Enviar("proyeccion.elemento", "{\"tipo\":\"versiculo\",\"modo\":\"tercio\"}");
        }

        private void txtCita_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Enter) { btnIr_Click(sender, e); e.Handled = true; }
        }
    }
}
