namespace FusionHP.Managed.Forms
{
    partial class BibleQuickForm
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null)) components.Dispose();
            base.Dispose(disposing);
        }

        private System.Windows.Forms.Label lblTitulo;
        private System.Windows.Forms.TextBox txtCita;
        private System.Windows.Forms.Button btnIr;
        private System.Windows.Forms.Button btnFavorito;
        private System.Windows.Forms.Button btnTercio;
        private System.Windows.Forms.TextBox txtResultado;

        private void InitializeComponent()
        {
            lblTitulo = new System.Windows.Forms.Label();
            txtCita = new System.Windows.Forms.TextBox();
            btnIr = new System.Windows.Forms.Button();
            btnFavorito = new System.Windows.Forms.Button();
            btnTercio = new System.Windows.Forms.Button();
            txtResultado = new System.Windows.Forms.TextBox();

            lblTitulo.AutoSize = true;
            lblTitulo.Font = new System.Drawing.Font("Outfit", 14F, System.Drawing.FontStyle.Bold);
            lblTitulo.Location = new System.Drawing.Point(12, 9);
            lblTitulo.Text = "Biblia rápida";

            txtCita.Location = new System.Drawing.Point(12, 40);
            txtCita.Size = new System.Drawing.Size(280, 23);
            txtCita.Text = "";
            txtCita.KeyDown += new System.Windows.Forms.KeyEventHandler(txtCita_KeyDown);

            btnIr.Text = "Ir (Enter)";
            btnIr.Location = new System.Drawing.Point(300, 39);
            btnIr.Size = new System.Drawing.Size(80, 25);
            btnIr.Click += new System.EventHandler(btnIr_Click);

            btnFavorito.Text = "★ Favorito";
            btnFavorito.Location = new System.Drawing.Point(390, 39);
            btnFavorito.Size = new System.Drawing.Size(100, 25);
            btnFavorito.Click += new System.EventHandler(btnFavorito_Click);

            btnTercio.Text = "Modo Tercio (lower)";
            btnTercio.Location = new System.Drawing.Point(500, 39);
            btnTercio.Size = new System.Drawing.Size(150, 25);
            btnTercio.Click += new System.EventHandler(btnTercio_Click);

            txtResultado.Location = new System.Drawing.Point(12, 75);
            txtResultado.Size = new System.Drawing.Size(640, 400);
            txtResultado.Multiline = true;
            txtResultado.Font = new System.Drawing.Font("Libre Baskerville", 14F);
            txtResultado.ReadOnly = true;

            ClientSize = new System.Drawing.Size(670, 500);
            Controls.Add(lblTitulo);
            Controls.Add(txtCita);
            Controls.Add(btnIr);
            Controls.Add(btnFavorito);
            Controls.Add(btnTercio);
            Controls.Add(txtResultado);
            Text = "FUSION-HP — Biblia rápida (G)";
            StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;

            ResumeLayout(false);
            PerformLayout();
        }
    }
}
