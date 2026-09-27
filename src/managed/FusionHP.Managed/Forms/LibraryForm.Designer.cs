namespace FusionHP.Managed.Forms
{
    partial class LibraryForm
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null)) components.Dispose();
            base.Dispose(disposing);
        }

        private System.Windows.Forms.Label lblCantosTitulo;
        private System.Windows.Forms.TextBox txtBuscarCanto;
        private System.Windows.Forms.ListBox lstCantos;
        private System.Windows.Forms.Label lblBibliaTitulo;
        private System.Windows.Forms.ComboBox cmbBiblias;
        private System.Windows.Forms.TreeView treeBiblia;
        private System.Windows.Forms.TextBox txtVersiculo;
        private System.Windows.Forms.Splitter splitter;

        private void InitializeComponent()
        {
            lblCantosTitulo = new System.Windows.Forms.Label();
            txtBuscarCanto = new System.Windows.Forms.TextBox();
            lstCantos = new System.Windows.Forms.ListBox();
            lblBibliaTitulo = new System.Windows.Forms.Label();
            cmbBiblias = new System.Windows.Forms.ComboBox();
            treeBiblia = new System.Windows.Forms.TreeView();
            txtVersiculo = new System.Windows.Forms.TextBox();

            lblCantosTitulo.AutoSize = true;
            lblCantosTitulo.Font = new System.Drawing.Font("Outfit", 12F, System.Drawing.FontStyle.Bold);
            lblCantosTitulo.Location = new System.Drawing.Point(12, 9);
            lblCantosTitulo.Text = "Cantos";

            txtBuscarCanto.Location = new System.Drawing.Point(12, 35);
            txtBuscarCanto.Size = new System.Drawing.Size(280, 23);
            txtBuscarCanto.PlaceholderText = "Buscar canto...";

            lstCantos.Location = new System.Drawing.Point(12, 64);
            lstCantos.Size = new System.Drawing.Size(280, 580);

            lblBibliaTitulo.AutoSize = true;
            lblBibliaTitulo.Font = new System.Drawing.Font("Outfit", 12F, System.Drawing.FontStyle.Bold);
            lblBibliaTitulo.Location = new System.Drawing.Point(310, 9);
            lblBibliaTitulo.Text = "Biblia";

            cmbBiblias.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            cmbBiblias.Location = new System.Drawing.Point(310, 35);
            cmbBiblias.Size = new System.Drawing.Size(280, 23);

            treeBiblia.Location = new System.Drawing.Point(310, 64);
            treeBiblia.Size = new System.Drawing.Size(280, 580);

            txtVersiculo.Location = new System.Drawing.Point(610, 64);
            txtVersiculo.Size = new System.Drawing.Size(580, 580);
            txtVersiculo.Multiline = true;
            txtVersiculo.Font = new System.Drawing.Font("Libre Baskerville", 12F);
            txtVersiculo.ReadOnly = true;
            txtVersiculo.Text = "Selecciona un versículo en el árbol.";

            ClientSize = new System.Drawing.Size(1200, 660);
            Controls.Add(lblCantosTitulo);
            Controls.Add(txtBuscarCanto);
            Controls.Add(lstCantos);
            Controls.Add(lblBibliaTitulo);
            Controls.Add(cmbBiblias);
            Controls.Add(treeBiblia);
            Controls.Add(txtVersiculo);
            Text = "FUSION-HP — Biblioteca";
            StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;

            ResumeLayout(false);
            PerformLayout();
        }
    }
}
