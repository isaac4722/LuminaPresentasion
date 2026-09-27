// src/managed/FusionHP.Managed/Forms/OperatorConsoleForm.cs — Modo Operador F8
//
// Consola en vivo: EN PANTALLA + SIGUIENTE, transporte grande,
// Negro/Logo/Ocultar, lista del programa con búsqueda, reloj y estado.

using System;
using System.Windows.Forms;
using FusionHP.Managed.Ipc;

namespace FusionHP.Managed.Forms
{
    public partial class OperatorConsoleForm : Form
    {
        private readonly IpcClient _ipc;
        private System.Windows.Forms.Timer _reloj;

        public OperatorConsoleForm(IpcClient ipc)
        {
            _ipc = ipc;
            InitializeComponent();
            IniciarReloj();
        }

        private void IniciarReloj()
        {
            _reloj = new System.Windows.Forms.Timer { Interval = 1000 };
            _reloj.Tick += (s, e) => { lblReloj.Text = DateTime.Now.ToString("HH:mm:ss"); };
            _reloj.Start();
        }

        private void btnSiguiente_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.siguiente", "{}");
        }

        private void btnAnterior_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.anterior", "{}");
        }

        private void btnNegro_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.negro", "{\"activar\":true}");
        }

        private void btnLogo_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.logo", "{\"activar\":true}");
        }

        private void btnOcultar_Click(object sender, EventArgs e)
        {
            _ipc.Enviar("proyeccion.detener", "{}");
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && _reloj != null) _reloj.Dispose();
            base.Dispose(disposing);
        }
    }
}
