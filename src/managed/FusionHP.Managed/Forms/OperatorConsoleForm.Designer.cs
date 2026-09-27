namespace FusionHP.Managed.Forms
{
    partial class OperatorConsoleForm
    {
        private System.ComponentModel.IContainer components;

        private System.Windows.Forms.Label lblReloj;
        private System.Windows.Forms.Label lblEnPantalla;
        private System.Windows.Forms.Label lblSiguiente;
        private System.Windows.Forms.Button btnAnterior;
        private System.Windows.Forms.Button btnSiguiente;
        private System.Windows.Forms.Button btnNegro;
        private System.Windows.Forms.Button btnLogo;
        private System.Windows.Forms.Button btnOcultar;
        private System.Windows.Forms.TextBox txtBuscarPrograma;
        private System.Windows.Forms.ListBox lstPrograma;
        private System.Windows.Forms.Label lblStatusSalida;

        private void InitializeComponent()
        {
            lblReloj = new System.Windows.Forms.Label();
            lblEnPantalla = new System.Windows.Forms.Label();
            lblSiguiente = new System.Windows.Forms.Label();
            btnAnterior = new System.Windows.Forms.Button();
            btnSiguiente = new System.Windows.Forms.Button();
            btnNegro = new System.Windows.Forms.Button();
            btnLogo = new System.Windows.Forms.Button();
            btnOcultar = new System.Windows.Forms.Button();
            txtBuscarPrograma = new System.Windows.Forms.TextBox();
            lstPrograma = new System.Windows.Forms.ListBox();
            lblStatusSalida = new System.Windows.Forms.Label();

            lblReloj.Font = new System.Drawing.Font("Outfit", 24F, System.Drawing.FontStyle.Bold);
            lblReloj.Location = new System.Drawing.Point(12, 9);
            lblReloj.Size = new System.Drawing.Size(180, 40);
            lblReloj.Text = "00:00:00";

            lblEnPantalla.AutoSize = true;
            lblEnPantalla.Font = new System.Drawing.Font("Outfit", 10F, System.Drawing.FontStyle.Bold);
            lblEnPantalla.Location = new System.Drawing.Point(200, 16);
            lblEnPantalla.Text = "EN PANTALLA: (nada)";

            lblSiguiente.AutoSize = true;
            lblSiguiente.Font = new System.Drawing.Font("Outfit", 10F);
            lblSiguiente.Location = new System.Drawing.Point(200, 36);
            lblSiguiente.Text = "SIGUIENTE: (nada)";

            btnAnterior.Text = "◀ Anterior";
            btnAnterior.Location = new System.Drawing.Point(12, 60);
            btnAnterior.Size = new System.Drawing.Size(120, 60);
            btnAnterior.Click += new System.EventHandler(btnAnterior_Click);

            btnSiguiente.Text = "Siguiente ▶";
            btnSiguiente.Location = new System.Drawing.Point(140, 60);
            btnSiguiente.Size = new System.Drawing.Size(120, 60);
            btnSiguiente.Click += new System.EventHandler(btnSiguiente_Click);

            btnNegro.Text = "Negro";
            btnNegro.Location = new System.Drawing.Point(270, 60);
            btnNegro.Size = new System.Drawing.Size(80, 60);
            btnNegro.Click += new System.EventHandler(btnNegro_Click);

            btnLogo.Text = "Logo";
            btnLogo.Location = new System.Drawing.Point(360, 60);
            btnLogo.Size = new System.Drawing.Size(80, 60);
            btnLogo.Click += new System.EventHandler(btnLogo_Click);

            btnOcultar.Text = "Ocultar";
            btnOcultar.Location = new System.Drawing.Point(450, 60);
            btnOcultar.Size = new System.Drawing.Size(80, 60);
            btnOcultar.Click += new System.EventHandler(btnOcultar_Click);

            txtBuscarPrograma.Location = new System.Drawing.Point(550, 60);
            txtBuscarPrograma.Size = new System.Drawing.Size(420, 23);
            txtBuscarPrograma.Text = "";

            lstPrograma.Location = new System.Drawing.Point(550, 90);
            lstPrograma.Size = new System.Drawing.Size(420, 500);

            lblStatusSalida.AutoSize = true;
            lblStatusSalida.Location = new System.Drawing.Point(12, 130);
            lblStatusSalida.Text = "Salida: oculta";

            ClientSize = new System.Drawing.Size(1000, 600);
            Controls.Add(lblReloj);
            Controls.Add(lblEnPantalla);
            Controls.Add(lblSiguiente);
            Controls.Add(btnAnterior);
            Controls.Add(btnSiguiente);
            Controls.Add(btnNegro);
            Controls.Add(btnLogo);
            Controls.Add(btnOcultar);
            Controls.Add(txtBuscarPrograma);
            Controls.Add(lstPrograma);
            Controls.Add(lblStatusSalida);
            Text = "FUSION-HP — Modo Operador (F8)";
            StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;

            ResumeLayout(false);
            PerformLayout();
        }
    }
}
