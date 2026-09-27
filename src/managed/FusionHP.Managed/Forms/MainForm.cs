// src/managed/FusionHP.Managed/Forms/MainForm.cs — Pantalla de Inicio
//
// Tarjetas, accesos y proyectos recientes con nombre visible.
// Biblioteca llena disponible desde el primer arranque.

using System;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class MainForm : Form
    {
        private readonly IpcClient _ipc;

        public MainForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            CargarRecientes();
            CargarConfiguracion();
        }

        private void CargarRecientes()
        {
            // TODO(P0): IPC programa.recientes
            // Por ahora, ejemplos visibles para verificar la UI.
            lstRecientes.Items.Clear();
            lstRecientes.Items.Add("Culto Domingo 2026-09-28");
            lstRecientes.Items.Add("Culto Domingo 2026-09-21");
            lstRecientes.Items.Add("Estudio Bíblico Miércoles");
        }

        private void CargarConfiguracion()
        {
            // TODO(P0): cargar runtime/config.json (monitores, logo de reposo, avance, reloj)
            lblConfig.Text = "Configuración: 2 monitores · Logo reposo: data/assets/logo/reposo.png";
        }

        private void btnEstudio_Click(object sender, EventArgs e)
        {
            using (var f = new StudioForm(_ipc)) f.ShowDialog(this);
        }

        private void btnPresentar_Click(object sender, EventArgs e)
        {
            using (var f = new PresentForm(_ipc)) f.ShowDialog(this);
        }

        private void btnBiblioteca_Click(object sender, EventArgs e)
        {
            using (var f = new LibraryForm(_ipc)) f.ShowDialog(this);
        }

        private void btnEstadoSistema_Click(object sender, EventArgs e)
        {
            using (var f = new DiagnosticForm(_ipc)) f.ShowDialog(this);
        }

        private void lstRecientes_DoubleClick(object sender, EventArgs e)
        {
            // TODO(P0): abrir el .ahp seleccionado
            if (lstRecientes.SelectedItem == null) return;
            using (var f = new StudioForm(_ipc)) f.ShowDialog(this);
        }
    }
}
