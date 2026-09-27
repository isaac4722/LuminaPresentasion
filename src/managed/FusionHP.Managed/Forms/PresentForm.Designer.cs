namespace FusionHP.Managed.Forms
{
    partial class PresentForm
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null)) components.Dispose();
            base.Dispose(disposing);
        }

        private System.Windows.Forms.Label lblTitulo;
        private System.Windows.Forms.ComboBox cmbMonitores;
        private System.Windows.Forms.Button btnIniciar;
        private System.Windows.Forms.Button btnDetener;
        private System.Windows.Forms.Button btnNegro;
        private System.Windows.Forms.Button btnLogo;
        private System.Windows.Forms.Panel pnlMiniaturas;
        private System.Windows.Forms.StatusStrip status;
        private System.Windows.Forms.ToolStripStatusLabel lblStatus;

        private void InitializeComponent()
        {
            lblTitulo = new System.Windows.Forms.Label();
            cmbMonitores = new System.Windows.Forms.ComboBox();
            btnIniciar = new System.Windows.Forms.Button();
            btnDetener = new System.Windows.Forms.Button();
            btnNegro = new System.Windows.Forms.Button();
            btnLogo = new System.Windows.Forms.Button();
            pnlMiniaturas = new System.Windows.Forms.Panel();
            status = new System.Windows.Forms.StatusStrip();
            lblStatus = new System.Windows.Forms.ToolStripStatusLabel();

            lblTitulo.AutoSize = true;
            lblTitulo.Font = new System.Drawing.Font("Outfit", 18F, System.Drawing.FontStyle.Bold);
            lblTitulo.Location = new System.Drawing.Point(12, 9);
            lblTitulo.Text = "Presentar";

            cmbMonitores.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            cmbMonitores.Location = new System.Drawing.Point(12, 45);
            cmbMonitores.Size = new System.Drawing.Size(280, 23);

            btnIniciar.Text = "Iniciar (F5)";
            btnIniciar.Location = new System.Drawing.Point(300, 44);
            btnIniciar.Size = new System.Drawing.Size(100, 25);
            btnIniciar.Click += new System.EventHandler(btnIniciar_Click);

            btnDetener.Text = "Detener (Esc)";
            btnDetener.Location = new System.Drawing.Point(410, 44);
            btnDetener.Size = new System.Drawing.Size(100, 25);
            btnDetener.Click += new System.EventHandler(btnDetener_Click);

            btnNegro.Text = "Negro (B)";
            btnNegro.Location = new System.Drawing.Point(520, 44);
            btnNegro.Size = new System.Drawing.Size(80, 25);
            btnNegro.Click += new System.EventHandler(btnNegro_Click);

            btnLogo.Text = "Logo (L)";
            btnLogo.Location = new System.Drawing.Point(610, 44);
            btnLogo.Size = new System.Drawing.Size(80, 25);
            btnLogo.Click += new System.EventHandler(btnLogo_Click);

            pnlMiniaturas.BackColor = System.Drawing.Color.DimGray;
            pnlMiniaturas.Location = new System.Drawing.Point(12, 80);
            pnlMiniaturas.Size = new System.Drawing.Size(980, 400);

            status.Items.Add(lblStatus);
            lblStatus.Text = "Salida: oculta";

            ClientSize = new System.Drawing.Size(1000, 500);
            Controls.Add(lblTitulo);
            Controls.Add(cmbMonitores);
            Controls.Add(btnIniciar);
            Controls.Add(btnDetener);
            Controls.Add(btnNegro);
            Controls.Add(btnLogo);
            Controls.Add(pnlMiniaturas);
            Controls.Add(status);
            Text = "FUSION-HP — Presentar";
            StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;

            ResumeLayout(false);
            PerformLayout();
        }
    }
}
