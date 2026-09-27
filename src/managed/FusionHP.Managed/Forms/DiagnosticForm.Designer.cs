namespace FusionHP.Managed.Forms
{
    partial class DiagnosticForm
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null)) components.Dispose();
            base.Dispose(disposing);
        }

        private System.Windows.Forms.Label lblTitulo;
        private System.Windows.Forms.Button btnAutotest;
        private System.Windows.Forms.TextBox txtResultados;

        private void InitializeComponent()
        {
            lblTitulo = new System.Windows.Forms.Label();
            btnAutotest = new System.Windows.Forms.Button();
            txtResultados = new System.Windows.Forms.TextBox();

            lblTitulo.AutoSize = true;
            lblTitulo.Font = new System.Drawing.Font("Outfit", 14F, System.Drawing.FontStyle.Bold);
            lblTitulo.Location = new System.Drawing.Point(12, 9);
            lblTitulo.Text = "Estado del sistema";

            btnAutotest.Text = "Ejecutar autotest";
            btnAutotest.Location = new System.Drawing.Point(12, 40);
            btnAutotest.Size = new System.Drawing.Size(180, 30);
            btnAutotest.Click += new System.EventHandler(btnAutotest_Click);

            txtResultados.Location = new System.Drawing.Point(12, 80);
            txtResultados.Size = new System.Drawing.Size(640, 360);
            txtResultados.Multiline = true;
            txtResultados.Font = new System.Drawing.Font("Consolas", 10F);
            txtResultados.ReadOnly = true;
            txtResultados.Text = "Pulsa \"Ejecutar autotest\".";

            ClientSize = new System.Drawing.Size(670, 460);
            Controls.Add(lblTitulo);
            Controls.Add(btnAutotest);
            Controls.Add(txtResultados);
            Text = "FUSION-HP — Estado del sistema";
            StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;

            ResumeLayout(false);
            PerformLayout();
        }
    }
}
