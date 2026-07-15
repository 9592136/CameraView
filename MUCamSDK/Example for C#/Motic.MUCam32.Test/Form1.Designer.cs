namespace Motic.MUCam32.Test
{
    partial class Form1
    {
        /// <summary>
        /// 必需的设计器变量。
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// 清理所有正在使用的资源。
        /// </summary>
        /// <param name="disposing">如果应释放托管资源，为 true；否则为 false。</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows 窗体设计器生成的代码

        /// <summary>
        /// 设计器支持所需的方法 - 不要
        /// 使用代码编辑器修改此方法的内容。
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.Device = new System.Windows.Forms.Label();
            this.Mirror = new System.Windows.Forms.CheckBox();
            this.Flip = new System.Windows.Forms.CheckBox();
            this.cmbBinning = new System.Windows.Forms.ComboBox();
            this.cmbDevice = new System.Windows.Forms.ComboBox();
            this.pictureBox = new System.Windows.Forms.PictureBox();
            this.videoTimer = new System.Windows.Forms.Timer(this.components);
            this.label1 = new System.Windows.Forms.Label();
            this.nudExposure = new System.Windows.Forms.NumericUpDown();
            this.textBoxOffset = new System.Windows.Forms.TextBox();
            this.trackBarOffset = new System.Windows.Forms.TrackBar();
            this.textBoxRedGain = new System.Windows.Forms.TextBox();
            this.trackBarRedGain = new System.Windows.Forms.TrackBar();
            this.textBoxGreenGain = new System.Windows.Forms.TextBox();
            this.trackBarGreenGain = new System.Windows.Forms.TrackBar();
            this.textBoxBlueGain = new System.Windows.Forms.TextBox();
            this.trackBarBlueGain = new System.Windows.Forms.TrackBar();
            this.label2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.label5 = new System.Windows.Forms.Label();
            this.label6 = new System.Windows.Forms.Label();
            this.groupBox1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.nudExposure)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.trackBarOffset)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.trackBarRedGain)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.trackBarGreenGain)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.trackBarBlueGain)).BeginInit();
            this.SuspendLayout();
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.label6);
            this.groupBox1.Controls.Add(this.label5);
            this.groupBox1.Controls.Add(this.label4);
            this.groupBox1.Controls.Add(this.label3);
            this.groupBox1.Controls.Add(this.label2);
            this.groupBox1.Controls.Add(this.label1);
            this.groupBox1.Controls.Add(this.Device);
            this.groupBox1.Controls.Add(this.Mirror);
            this.groupBox1.Controls.Add(this.Flip);
            this.groupBox1.Controls.Add(this.trackBarBlueGain);
            this.groupBox1.Controls.Add(this.textBoxBlueGain);
            this.groupBox1.Controls.Add(this.trackBarGreenGain);
            this.groupBox1.Controls.Add(this.textBoxGreenGain);
            this.groupBox1.Controls.Add(this.trackBarRedGain);
            this.groupBox1.Controls.Add(this.textBoxRedGain);
            this.groupBox1.Controls.Add(this.trackBarOffset);
            this.groupBox1.Controls.Add(this.textBoxOffset);
            this.groupBox1.Controls.Add(this.nudExposure);
            this.groupBox1.Controls.Add(this.cmbBinning);
            this.groupBox1.Controls.Add(this.cmbDevice);
            this.groupBox1.Location = new System.Drawing.Point(496, 12);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(225, 481);
            this.groupBox1.TabIndex = 3;
            this.groupBox1.TabStop = false;
            // 
            // Device
            // 
            this.Device.AutoSize = true;
            this.Device.Location = new System.Drawing.Point(13, 29);
            this.Device.Name = "Device";
            this.Device.Size = new System.Drawing.Size(41, 12);
            this.Device.TabIndex = 9;
            this.Device.Text = "Device";
            // 
            // Mirror
            // 
            this.Mirror.AutoSize = true;
            this.Mirror.Location = new System.Drawing.Point(108, 379);
            this.Mirror.Name = "Mirror";
            this.Mirror.Size = new System.Drawing.Size(60, 16);
            this.Mirror.TabIndex = 8;
            this.Mirror.Text = "Mirror";
            this.Mirror.UseVisualStyleBackColor = true;
            this.Mirror.CheckedChanged += new System.EventHandler(this.Mirror_CheckedChanged);
            // 
            // Flip
            // 
            this.Flip.AutoSize = true;
            this.Flip.Location = new System.Drawing.Point(15, 379);
            this.Flip.Name = "Flip";
            this.Flip.Size = new System.Drawing.Size(48, 16);
            this.Flip.TabIndex = 8;
            this.Flip.Text = "Flip";
            this.Flip.UseVisualStyleBackColor = true;
            this.Flip.CheckedChanged += new System.EventHandler(this.Flip_CheckedChanged);
            // 
            // cmbBinning
            // 
            this.cmbBinning.FormattingEnabled = true;
            this.cmbBinning.Location = new System.Drawing.Point(91, 50);
            this.cmbBinning.Name = "cmbBinning";
            this.cmbBinning.Size = new System.Drawing.Size(121, 20);
            this.cmbBinning.TabIndex = 3;
            this.cmbBinning.SelectedIndexChanged += new System.EventHandler(this.cmbBinning_SelectedIndexChanged);
            // 
            // cmbDevice
            // 
            this.cmbDevice.FormattingEnabled = true;
            this.cmbDevice.Location = new System.Drawing.Point(91, 21);
            this.cmbDevice.Name = "cmbDevice";
            this.cmbDevice.Size = new System.Drawing.Size(121, 20);
            this.cmbDevice.TabIndex = 1;
            this.cmbDevice.SelectedIndexChanged += new System.EventHandler(this.cmbDevice_SelectedIndexChanged);
            // 
            // pictureBox
            // 
            this.pictureBox.Location = new System.Drawing.Point(12, 12);
            this.pictureBox.Name = "pictureBox";
            this.pictureBox.Size = new System.Drawing.Size(459, 472);
            this.pictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.pictureBox.TabIndex = 4;
            this.pictureBox.TabStop = false;
            // 
            // videoTimer
            // 
            this.videoTimer.Tick += new System.EventHandler(this.videoTimer_Tick);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(13, 53);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(47, 12);
            this.label1.TabIndex = 9;
            this.label1.Text = "Binning";
            // 
            // nudExposure
            // 
            this.nudExposure.Location = new System.Drawing.Point(91, 89);
            this.nudExposure.Name = "nudExposure";
            this.nudExposure.Size = new System.Drawing.Size(120, 21);
            this.nudExposure.TabIndex = 5;
            this.nudExposure.ValueChanged += new System.EventHandler(this.nudExposure_ValueChanged);
            // 
            // textBoxOffset
            // 
            this.textBoxOffset.Location = new System.Drawing.Point(91, 116);
            this.textBoxOffset.Name = "textBoxOffset";
            this.textBoxOffset.ReadOnly = true;
            this.textBoxOffset.Size = new System.Drawing.Size(73, 21);
            this.textBoxOffset.TabIndex = 6;
            // 
            // trackBarOffset
            // 
            this.trackBarOffset.Location = new System.Drawing.Point(15, 141);
            this.trackBarOffset.Name = "trackBarOffset";
            this.trackBarOffset.Size = new System.Drawing.Size(197, 45);
            this.trackBarOffset.TabIndex = 7;
            this.trackBarOffset.Scroll += new System.EventHandler(this.trackBarOffset_Scroll);
            // 
            // textBoxRedGain
            // 
            this.textBoxRedGain.Location = new System.Drawing.Point(90, 179);
            this.textBoxRedGain.Name = "textBoxRedGain";
            this.textBoxRedGain.ReadOnly = true;
            this.textBoxRedGain.Size = new System.Drawing.Size(73, 21);
            this.textBoxRedGain.TabIndex = 6;
            // 
            // trackBarRedGain
            // 
            this.trackBarRedGain.Location = new System.Drawing.Point(15, 206);
            this.trackBarRedGain.Name = "trackBarRedGain";
            this.trackBarRedGain.Size = new System.Drawing.Size(196, 45);
            this.trackBarRedGain.TabIndex = 7;
            this.trackBarRedGain.Scroll += new System.EventHandler(this.trackBarRedGain_Scroll);
            // 
            // textBoxGreenGain
            // 
            this.textBoxGreenGain.Location = new System.Drawing.Point(90, 243);
            this.textBoxGreenGain.Name = "textBoxGreenGain";
            this.textBoxGreenGain.ReadOnly = true;
            this.textBoxGreenGain.Size = new System.Drawing.Size(73, 21);
            this.textBoxGreenGain.TabIndex = 6;
            // 
            // trackBarGreenGain
            // 
            this.trackBarGreenGain.Location = new System.Drawing.Point(15, 270);
            this.trackBarGreenGain.Name = "trackBarGreenGain";
            this.trackBarGreenGain.Size = new System.Drawing.Size(196, 45);
            this.trackBarGreenGain.TabIndex = 7;
            this.trackBarGreenGain.Scroll += new System.EventHandler(this.trackBarGreenGain_Scroll);
            // 
            // textBoxBlueGain
            // 
            this.textBoxBlueGain.Location = new System.Drawing.Point(90, 315);
            this.textBoxBlueGain.Name = "textBoxBlueGain";
            this.textBoxBlueGain.ReadOnly = true;
            this.textBoxBlueGain.Size = new System.Drawing.Size(73, 21);
            this.textBoxBlueGain.TabIndex = 6;
            // 
            // trackBarBlueGain
            // 
            this.trackBarBlueGain.Location = new System.Drawing.Point(15, 336);
            this.trackBarBlueGain.Name = "trackBarBlueGain";
            this.trackBarBlueGain.Size = new System.Drawing.Size(196, 45);
            this.trackBarBlueGain.TabIndex = 7;
            this.trackBarBlueGain.Scroll += new System.EventHandler(this.trackBarBlueGain_Scroll);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(13, 89);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(53, 12);
            this.label2.TabIndex = 9;
            this.label2.Text = "Exposure";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(13, 116);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(41, 12);
            this.label3.TabIndex = 9;
            this.label3.Text = "Offset";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(13, 182);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(53, 12);
            this.label4.TabIndex = 9;
            this.label4.Text = "Red Gain";
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(13, 246);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(65, 12);
            this.label5.TabIndex = 9;
            this.label5.Text = "Green Gain";
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(13, 321);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(59, 12);
            this.label6.TabIndex = 9;
            this.label6.Text = "Blue Gain";
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 12F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(723, 505);
            this.Controls.Add(this.pictureBox);
            this.Controls.Add(this.groupBox1);
            this.Name = "Form1";
            this.Text = "Form1";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.Form1_FormClosed);
            this.Load += new System.EventHandler(this.Form1_Load);
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.nudExposure)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.trackBarOffset)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.trackBarRedGain)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.trackBarGreenGain)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.trackBarBlueGain)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.Label Device;
        private System.Windows.Forms.CheckBox Mirror;
        private System.Windows.Forms.CheckBox Flip;
        private System.Windows.Forms.ComboBox cmbBinning;
        private System.Windows.Forms.ComboBox cmbDevice;
        private System.Windows.Forms.PictureBox pictureBox;
        private System.Windows.Forms.Timer videoTimer;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TrackBar trackBarBlueGain;
        private System.Windows.Forms.TextBox textBoxBlueGain;
        private System.Windows.Forms.TrackBar trackBarGreenGain;
        private System.Windows.Forms.TextBox textBoxGreenGain;
        private System.Windows.Forms.TrackBar trackBarRedGain;
        private System.Windows.Forms.TextBox textBoxRedGain;
        private System.Windows.Forms.TrackBar trackBarOffset;
        private System.Windows.Forms.TextBox textBoxOffset;
        private System.Windows.Forms.NumericUpDown nudExposure;
    }
}

