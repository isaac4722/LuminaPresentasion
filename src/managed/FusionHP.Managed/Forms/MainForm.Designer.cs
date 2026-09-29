// src/managed/FusionHP.Managed/Forms/MainForm.Designer.cs

namespace FusionHP.Managed.Forms
{
    partial class MainForm
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null)) components.Dispose();
            base.Dispose(disposing);
        }

        private System.Windows.Forms.Label lblTitulo;
        private System.Windows.Forms.Button btnEstudio;
        private System.Windows.Forms.Button btnPresentar;
        private System.Windows.Forms.Button btnBiblioteca;
        private System.Windows.Forms.Button btnEstadoSistema;
        private System.Windows.Forms.Button btnAbrirPrograma;
        private System.Windows.Forms.Label lblRecientesTitulo;
        private System.Windows.Forms.ListBox lstRecientes;
        private System.Windows.Forms.Label lblConfig;

        private void InitializeComponent()
        {
            lblTitulo = new System.Windows.Forms.Label();
            btnEstudio = new System.Windows.Forms.Button();
            btnPresentar = new System.Windows.Forms.Button();
            btnBiblioteca = new System.Windows.Forms.Button();
            btnEstadoSistema = new System.Windows.Forms.Button();
            btnAbrirPrograma = new System.Windows.Forms.Button();
            lblRecientesTitulo = new System.Windows.Forms.Label();
            lstRecientes = new System.Windows.Forms.ListBox();
            lblConfig = new System.Windows.Forms.Label();
            SuspendLayout();

            lblTitulo.AutoSize = true;
            lblTitulo.Font = new System.Drawing.Font("Outfit", 28F, System.Drawing.FontStyle.Bold);
            lblTitulo.Location = new System.Drawing.Point(24, 16);
            lblTitulo.Text = "FUSION-HP";

            btnEstudio.Text = "Estudio";
            btnEstudio.Location = new System.Drawing.Point(24, 80);
            btnEstudio.Size = new System.Drawing.Size(200, 80);
            btnEstudio.Click += new System.EventHandler(btnEstudio_Click);

            btnPresentar.Text = "Presentar";
            btnPresentar.Location = new System.Drawing.Point(240, 80);
            btnPresentar.Size = new System.Drawing.Size(200, 80);
            btnPresentar.Click += new System.EventHandler(btnPresentar_Click);

            btnBiblioteca.Text = "Biblioteca";
            btnBiblioteca.Location = new System.Drawing.Point(456, 80);
            btnBiblioteca.Size = new System.Drawing.Size(200, 80);
            btnBiblioteca.Click += new System.EventHandler(btnBiblioteca_Click);

            btnEstadoSistema.Text = "Estado del sistema";
            btnEstadoSistema.Location = new System.Drawing.Point(672, 80);
            btnEstadoSistema.Size = new System.Drawing.Size(200, 80);
            btnEstadoSistema.Click += new System.EventHandler(btnEstadoSistema_Click);

            btnAbrirPrograma.Text = "Abrir programa (.ahp)...";
            btnAbrirPrograma.Location = new System.Drawing.Point(24, 170);
            btnAbrirPrograma.Size = new System.Drawing.Size(200, 28);
            btnAbrirPrograma.Click += new System.EventHandler(btnAbrirPrograma_Click);

            lblRecientesTitulo.AutoSize = true;
            lblRecientesTitulo.Font = new System.Drawing.Font("Outfit", 14F);
            lblRecientesTitulo.Location = new System.Drawing.Point(24, 206);
            lblRecientesTitulo.Text = "Recientes";

            lstRecientes.Location = new System.Drawing.Point(24, 238);
            lstRecientes.Size = new System.Drawing.Size(848, 200);
            lstRecientes.DoubleClick += new System.EventHandler(lstRecientes_DoubleClick);

            lblConfig.AutoSize = true;
            lblConfig.Location = new System.Drawing.Point(24, 460);
            lblConfig.Text = "Núcleo: (conectando...)";

            ClientSize = new System.Drawing.Size(900, 500);
            Controls.Add(lblTitulo);
            Controls.Add(btnEstudio);
            Controls.Add(btnPresentar);
            Controls.Add(btnBiblioteca);
            Controls.Add(btnEstadoSistema);
            Controls.Add(btnAbrirPrograma);
            Controls.Add(lblRecientesTitulo);
            Controls.Add(lstRecientes);
            Controls.Add(lblConfig);
            Text = "FUSION-HP — Inicio";
            StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;

            ResumeLayout(false);
            PerformLayout();
        }
    }
}
