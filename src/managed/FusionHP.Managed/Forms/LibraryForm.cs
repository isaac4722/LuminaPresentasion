// src/managed/FusionHP.Managed/Forms/LibraryForm.cs — Biblioteca
//
// Cantos en una sola BD (cancionero.fdb), cargada una vez, lista completa.
// 4 biblias preinstaladas con árbol de 66 libros siempre visible.

using System;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class LibraryForm : Form
    {
        private readonly IpcClient _ipc;

        public LibraryForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            CargarCantos();
            CargarArbolBiblia();
            CargarBiblias();
        }

        private void CargarCantos()
        {
            // TODO(P0): IPC canto.listar
            lstCantos.Items.Clear();
            lstCantos.Items.Add("Cuán Grande Es Él");
            lstCantos.Items.Add("A Dios Sea La Gloria");
            lstCantos.Items.Add("Grande Es Tu Fidelidad");
        }

        private void CargarArbolBiblia()
        {
            // Árbol de 66 libros siempre visible (esquema bible.sql los trae 1..66).
            treeBiblia.Nodes.Clear();
            // TODO(P0): IPC biblia.listar -> cargar libros, capitulos, versiculos
            var atNode = treeBiblia.Nodes.Add("Antiguo Testamento");
            for (int i = 1; i <= 39; i++) atNode.Nodes.Add("Libro " + i);
            var ntNode = treeBiblia.Nodes.Add("Nuevo Testamento");
            for (int i = 40; i <= 66; i++) ntNode.Nodes.Add("Libro " + i);
            treeBiblia.ExpandAll();
        }

        private void CargarBiblias()
        {
            cmbBiblias.Items.Clear();
            cmbBiblias.Items.Add("RVR1909 — Reina-Valera 1909");
            cmbBiblias.Items.Add("RVG — Reina-Valera Gómez 2010");
            cmbBiblias.Items.Add("RV1960 — Reina-Valera 1960");
            cmbBiblias.Items.Add("NVI — Nueva Versión Internacional");
            if (cmbBiblias.Items.Count > 0) cmbBiblias.SelectedIndex = 0;
        }
    }
}
