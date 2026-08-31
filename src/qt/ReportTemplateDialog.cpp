#include "ReportTemplateDialog.h"

#include "app/ExportActions.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSplitter>
#include <QStringConverter>
#include <QTextBrowser>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <filesystem>

namespace {

constexpr int kSectionRole = Qt::UserRole;
constexpr int kHeadingRole = Qt::UserRole + 1;

const std::array<ImageReportTemplateSection, 6> kDefaultSectionOrder = {
    ImageReportTemplateSection::CurrentImage,
    ImageReportTemplateSection::ReportInformation,
    ImageReportTemplateSection::ReportNotes,
    ImageReportTemplateSection::MeasurementSummary,
    ImageReportTemplateSection::MeasurementTable,
    ImageReportTemplateSection::ImageDetails};

QString SectionName(ImageReportTemplateSection section)
{
    switch (section) {
    case ImageReportTemplateSection::CurrentImage: return QObject::tr("当前图像");
    case ImageReportTemplateSection::ReportInformation: return QObject::tr("报告信息");
    case ImageReportTemplateSection::ReportNotes: return QObject::tr("报告备注");
    case ImageReportTemplateSection::MeasurementSummary: return QObject::tr("测量摘要");
    case ImageReportTemplateSection::MeasurementTable: return QObject::tr("测量明细");
    case ImageReportTemplateSection::ImageDetails: return QObject::tr("图像与处理详情");
    }
    return {};
}

QString DefaultHeading(ImageReportTemplateSection section)
{
    switch (section) {
    case ImageReportTemplateSection::CurrentImage: return QStringLiteral("Current Image");
    case ImageReportTemplateSection::ReportInformation: return QStringLiteral("Report Information");
    case ImageReportTemplateSection::ReportNotes: return QStringLiteral("Notes");
    case ImageReportTemplateSection::MeasurementSummary: return QStringLiteral("Measurement Summary");
    case ImageReportTemplateSection::MeasurementTable: return QStringLiteral("Measurements");
    case ImageReportTemplateSection::ImageDetails: return QStringLiteral("Image Details");
    }
    return {};
}

QString HeadingFor(const ImageReportTemplateOptions& options, ImageReportTemplateSection section)
{
    const std::wstring* heading = nullptr;
    switch (section) {
    case ImageReportTemplateSection::CurrentImage: heading = &options.current_image_heading; break;
    case ImageReportTemplateSection::ReportInformation: heading = &options.report_information_heading; break;
    case ImageReportTemplateSection::ReportNotes: heading = &options.notes_heading; break;
    case ImageReportTemplateSection::MeasurementSummary: heading = &options.measurement_summary_heading; break;
    case ImageReportTemplateSection::MeasurementTable: heading = &options.measurement_table_heading; break;
    case ImageReportTemplateSection::ImageDetails: heading = &options.image_details_heading; break;
    }
    return heading && !heading->empty() ? QString::fromStdWString(*heading) : DefaultHeading(section);
}

bool SectionVisible(const ImageReportTemplateOptions& options, ImageReportTemplateSection section)
{
    switch (section) {
    case ImageReportTemplateSection::CurrentImage: return options.show_image;
    case ImageReportTemplateSection::ReportInformation: return options.show_report_information;
    case ImageReportTemplateSection::ReportNotes: return options.show_notes;
    case ImageReportTemplateSection::MeasurementSummary: return options.show_measurement_summary;
    case ImageReportTemplateSection::MeasurementTable: return options.show_measurement_table;
    case ImageReportTemplateSection::ImageDetails:
        return options.show_calibration_details || options.show_processing_details;
    }
    return true;
}

void SetSectionHeading(
    ImageReportTemplateOptions& options,
    ImageReportTemplateSection section,
    const QString& heading)
{
    const std::wstring value = heading.toStdWString();
    switch (section) {
    case ImageReportTemplateSection::CurrentImage: options.current_image_heading = value; break;
    case ImageReportTemplateSection::ReportInformation: options.report_information_heading = value; break;
    case ImageReportTemplateSection::ReportNotes: options.notes_heading = value; break;
    case ImageReportTemplateSection::MeasurementSummary: options.measurement_summary_heading = value; break;
    case ImageReportTemplateSection::MeasurementTable: options.measurement_table_heading = value; break;
    case ImageReportTemplateSection::ImageDetails: options.image_details_heading = value; break;
    }
}

QString PreviewHtml(std::wstring html)
{
    QString preview = QString::fromStdWString(html);
    preview.replace(QStringLiteral("{{ImageTag}}"),
        QStringLiteral("<div style='height:180px;border:1px dashed #7f98b3;display:flex;"
                       "align-items:center;justify-content:center;background:#111820;color:#9db6cf'>"
                       "当前图像预览</div>"));
    preview.replace(QStringLiteral("{{Generated}}"), QStringLiteral("2026-08-14 10:30:00"));
    preview.replace(QStringLiteral("{{MeasurementSummary}}"),
        QStringLiteral("<dl class='summary-grid'><dt>Total measurements</dt><dd>6</dd>"
                       "<dt>Calibrated</dt><dd>Yes</dd></dl>"));
    preview.replace(QStringLiteral("{{MeasurementTable}}"),
        QStringLiteral("<table class='measurement-table'><tr><th>Name</th><th>Type</th><th>Value</th></tr>"
                       "<tr><td>Length 1</td><td>Length</td><td>125.40 µm</td></tr>"
                       "<tr><td>Circle 1</td><td>Circle</td><td>Ø 48.20 µm</td></tr></table>"));
    const QStringList tokens = {
        QStringLiteral("Objective"), QStringLiteral("Calibrated"), QStringLiteral("MicronsPerPixel"),
        QStringLiteral("DisplayUnit"), QStringLiteral("LatestFrameSource"), QStringLiteral("ImageSize"),
        QStringLiteral("ViewportZoom"), QStringLiteral("PseudoColor"), QStringLiteral("FluorescenceChannels"),
        QStringLiteral("StitchTiles"), QStringLiteral("StitchResultBackend"),
        QStringLiteral("StitchResultRegistration"), QStringLiteral("StitchResultTilePositions"),
        QStringLiteral("EdfFrames")};
    for (const QString& token : tokens) {
        preview.replace(QStringLiteral("{{%1}}").arg(token), QStringLiteral("—"));
    }
    return preview;
}

} // namespace

ReportTemplateDialog::ReportTemplateDialog(
    const ImageReportTemplateOptions& options,
    QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("ReportTemplateDialog"));
    setWindowTitle(tr("报告模板设计"));
    setMinimumSize(900, 620);
    resize(1120, 760);

    auto* root = new QVBoxLayout(this);
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* settings_container = new QWidget;
    auto* settings_layout = new QVBoxLayout(settings_container);
    settings_layout->setContentsMargins(8, 8, 8, 8);

    auto* basic_group = new QGroupBox(tr("页面与标题"));
    auto* basic_form = new QFormLayout(basic_group);
    title_edit_ = new QLineEdit;
    title_edit_->setObjectName(QStringLiteral("ReportTemplateTitle"));
    subtitle_edit_ = new QLineEdit;
    accent_combo_ = new QComboBox;
    accent_combo_->setObjectName(QStringLiteral("ReportTemplateAccent"));
    accent_combo_->addItems({tr("蓝色"), tr("绿色"), tr("金色"), tr("洋红")});
    image_size_combo_ = new QComboBox;
    image_size_combo_->addItems({tr("原始尺寸"), tr("适合页面"), tr("紧凑")});
    page_layout_combo_ = new QComboBox;
    page_layout_combo_->addItems({tr("标准"), tr("宽版"), tr("紧凑")});
    orientation_combo_ = new QComboBox;
    orientation_combo_->addItems({tr("纵向"), tr("横向")});
    precision_combo_ = new QComboBox;
    precision_combo_->addItems({tr("自动"), tr("两位小数"), tr("三位小数")});
    image_caption_edit_ = new QLineEdit;
    footer_edit_ = new QLineEdit;
    basic_form->addRow(tr("报告标题"), title_edit_);
    basic_form->addRow(tr("副标题"), subtitle_edit_);
    basic_form->addRow(tr("强调色"), accent_combo_);
    basic_form->addRow(tr("图像尺寸"), image_size_combo_);
    basic_form->addRow(tr("页面布局"), page_layout_combo_);
    basic_form->addRow(tr("打印方向"), orientation_combo_);
    basic_form->addRow(tr("测量精度"), precision_combo_);
    basic_form->addRow(tr("图像说明"), image_caption_edit_);
    basic_form->addRow(tr("页脚文字"), footer_edit_);
    settings_layout->addWidget(basic_group);

    auto* content_group = new QGroupBox(tr("报告内容"));
    auto* content_layout = new QVBoxLayout(content_group);
    raw_values_check_ = new QCheckBox(tr("测量明细包含原始像素值"));
    group_measurements_check_ = new QCheckBox(tr("按测量类型分组"));
    calibration_check_ = new QCheckBox(tr("包含标定与物镜信息"));
    processing_check_ = new QCheckBox(tr("包含图像处理信息"));
    footer_check_ = new QCheckBox(tr("显示页脚"));
    content_layout->addWidget(raw_values_check_);
    content_layout->addWidget(group_measurements_check_);
    content_layout->addWidget(calibration_check_);
    content_layout->addWidget(processing_check_);
    content_layout->addWidget(footer_check_);
    content_layout->addWidget(new QLabel(tr("报告信息（每行“字段: 内容”，支持报告占位符）")));
    information_edit_ = new QPlainTextEdit;
    information_edit_->setMaximumHeight(90);
    content_layout->addWidget(information_edit_);
    content_layout->addWidget(new QLabel(tr("报告备注")));
    notes_edit_ = new QPlainTextEdit;
    notes_edit_->setMaximumHeight(90);
    content_layout->addWidget(notes_edit_);
    settings_layout->addWidget(content_group);

    auto* section_group = new QGroupBox(tr("章节顺序与标题"));
    auto* section_layout = new QVBoxLayout(section_group);
    section_list_ = new QListWidget;
    section_list_->setObjectName(QStringLiteral("ReportTemplateSectionList"));
    section_list_->setMinimumHeight(170);
    section_layout->addWidget(section_list_);
    auto* section_buttons = new QHBoxLayout;
    auto* up_button = new QPushButton(tr("上移"));
    auto* down_button = new QPushButton(tr("下移"));
    section_buttons->addWidget(up_button);
    section_buttons->addWidget(down_button);
    section_buttons->addStretch();
    section_layout->addLayout(section_buttons);
    auto* heading_form = new QFormLayout;
    section_heading_edit_ = new QLineEdit;
    heading_form->addRow(tr("所选章节标题"), section_heading_edit_);
    section_layout->addLayout(heading_form);
    settings_layout->addWidget(section_group);
    settings_layout->addStretch();

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(settings_container);
    splitter->addWidget(scroll);

    auto* preview_container = new QWidget;
    auto* preview_layout = new QVBoxLayout(preview_container);
    preview_layout->addWidget(new QLabel(tr("实时预览")));
    preview_ = new QTextBrowser;
    preview_->setObjectName(QStringLiteral("ReportTemplatePreview"));
    preview_->setOpenExternalLinks(false);
    preview_layout->addWidget(preview_, 1);
    splitter->addWidget(preview_container);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({420, 680});
    root->addWidget(splitter, 1);

    auto* action_row = new QHBoxLayout;
    auto* load_button = new QPushButton(tr("载入模板…"));
    auto* save_button = new QPushButton(tr("另存模板…"));
    auto* reset_button = new QPushButton(tr("恢复默认"));
    action_row->addWidget(load_button);
    action_row->addWidget(save_button);
    action_row->addWidget(reset_button);
    action_row->addStretch();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    action_row->addWidget(buttons);
    root->addLayout(action_row);

    applyOptions(options);

    const auto update = [this] { updatePreview(); };
    for (QLineEdit* edit : {title_edit_, subtitle_edit_, image_caption_edit_, footer_edit_}) {
        connect(edit, &QLineEdit::textChanged, this, update);
    }
    for (QComboBox* combo : {accent_combo_, image_size_combo_, page_layout_combo_, orientation_combo_, precision_combo_}) {
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, update);
    }
    for (QCheckBox* check : {raw_values_check_, group_measurements_check_, calibration_check_, processing_check_, footer_check_}) {
        connect(check, &QCheckBox::toggled, this, update);
    }
    connect(information_edit_, &QPlainTextEdit::textChanged, this, update);
    connect(notes_edit_, &QPlainTextEdit::textChanged, this, update);
    connect(section_list_, &QListWidget::currentRowChanged, this, [this] { refreshSectionEditor(); });
    connect(section_list_, &QListWidget::itemChanged, this, [this] { updatePreview(); });
    connect(section_heading_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (QListWidgetItem* item = section_list_->currentItem()) {
            item->setData(kHeadingRole, text);
            updatePreview();
        }
    });
    connect(up_button, &QPushButton::clicked, this, [this] { moveCurrentSection(-1); });
    connect(down_button, &QPushButton::clicked, this, [this] { moveCurrentSection(1); });
    connect(load_button, &QPushButton::clicked, this, &ReportTemplateDialog::loadTemplate);
    connect(save_button, &QPushButton::clicked, this, &ReportTemplateDialog::saveTemplate);
    connect(reset_button, &QPushButton::clicked, this, [this] { applyOptions(ImageReportTemplateOptions{}); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updatePreview();
}

ImageReportTemplateOptions ReportTemplateDialog::options() const
{
    ImageReportTemplateOptions result;
    result.title = title_edit_->text().trimmed().toStdWString();
    result.subtitle = subtitle_edit_->text().toStdWString();
    result.accent = static_cast<ImageReportTemplateAccent>(accent_combo_->currentIndex());
    result.image_size = static_cast<ImageReportTemplateImageSize>(image_size_combo_->currentIndex());
    result.page_layout = static_cast<ImageReportTemplatePageLayout>(page_layout_combo_->currentIndex());
    result.print_orientation = static_cast<ImageReportTemplatePrintOrientation>(orientation_combo_->currentIndex());
    result.measurement_precision = static_cast<ImageReportTemplateMeasurementPrecision>(precision_combo_->currentIndex());
    result.image_caption = image_caption_edit_->text().toStdWString();
    result.footer_text = footer_edit_->text().toStdWString();
    result.show_measurement_raw_values = raw_values_check_->isChecked();
    result.group_measurements_by_type = group_measurements_check_->isChecked();
    result.show_calibration_details = calibration_check_->isChecked();
    result.show_processing_details = processing_check_->isChecked();
    result.show_footer = footer_check_->isChecked();
    result.report_information_fields = information_edit_->toPlainText().toStdWString();
    result.notes = notes_edit_->toPlainText().toStdWString();

    for (int row = 0; row < section_list_->count(); ++row) {
        const QListWidgetItem* item = section_list_->item(row);
        const auto section = static_cast<ImageReportTemplateSection>(item->data(kSectionRole).toInt());
        const bool visible = item->checkState() == Qt::Checked;
        result.section_order.push_back(section);
        SetSectionHeading(result, section, item->data(kHeadingRole).toString());
        switch (section) {
        case ImageReportTemplateSection::CurrentImage: result.show_image = visible; break;
        case ImageReportTemplateSection::ReportInformation: result.show_report_information = visible; break;
        case ImageReportTemplateSection::ReportNotes: result.show_notes = visible; break;
        case ImageReportTemplateSection::MeasurementSummary: result.show_measurement_summary = visible; break;
        case ImageReportTemplateSection::MeasurementTable: result.show_measurement_table = visible; break;
        case ImageReportTemplateSection::ImageDetails:
            if (!visible) {
                result.show_calibration_details = false;
                result.show_processing_details = false;
            }
            break;
        }
    }
    return result;
}

void ReportTemplateDialog::applyOptions(const ImageReportTemplateOptions& options)
{
    title_edit_->setText(QString::fromStdWString(options.title));
    subtitle_edit_->setText(QString::fromStdWString(options.subtitle));
    accent_combo_->setCurrentIndex(static_cast<int>(options.accent));
    image_size_combo_->setCurrentIndex(static_cast<int>(options.image_size));
    page_layout_combo_->setCurrentIndex(static_cast<int>(options.page_layout));
    orientation_combo_->setCurrentIndex(static_cast<int>(options.print_orientation));
    precision_combo_->setCurrentIndex(static_cast<int>(options.measurement_precision));
    image_caption_edit_->setText(QString::fromStdWString(options.image_caption));
    footer_edit_->setText(QString::fromStdWString(options.footer_text));
    raw_values_check_->setChecked(options.show_measurement_raw_values);
    group_measurements_check_->setChecked(options.group_measurements_by_type);
    calibration_check_->setChecked(options.show_calibration_details);
    processing_check_->setChecked(options.show_processing_details);
    footer_check_->setChecked(options.show_footer);
    information_edit_->setPlainText(QString::fromStdWString(options.report_information_fields));
    notes_edit_->setPlainText(QString::fromStdWString(options.notes));

    section_list_->clear();
    std::vector<ImageReportTemplateSection> order = options.section_order;
    for (ImageReportTemplateSection section : kDefaultSectionOrder) {
        if (std::find(order.begin(), order.end(), section) == order.end()) {
            order.push_back(section);
        }
    }
    for (ImageReportTemplateSection section : order) {
        auto* item = new QListWidgetItem(SectionName(section), section_list_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(SectionVisible(options, section) ? Qt::Checked : Qt::Unchecked);
        item->setData(kSectionRole, static_cast<int>(section));
        item->setData(kHeadingRole, HeadingFor(options, section));
    }
    if (section_list_->count() > 0) {
        section_list_->setCurrentRow(0);
    }
    refreshSectionEditor();
    updatePreview();
}

void ReportTemplateDialog::refreshSectionEditor()
{
    const QListWidgetItem* item = section_list_->currentItem();
    section_heading_edit_->setEnabled(item != nullptr);
    section_heading_edit_->setText(item ? item->data(kHeadingRole).toString() : QString{});
}

void ReportTemplateDialog::updatePreview()
{
    if (!preview_ || !title_edit_) {
        return;
    }
    preview_->setHtml(PreviewHtml(DiagnosticReportActions::BuildImageReportTemplate(options())));
}

void ReportTemplateDialog::moveCurrentSection(int delta)
{
    const int row = section_list_->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= section_list_->count()) {
        return;
    }
    QListWidgetItem* item = section_list_->takeItem(row);
    section_list_->insertItem(target, item);
    section_list_->setCurrentRow(target);
    updatePreview();
}

void ReportTemplateDialog::loadTemplate()
{
    const QString file_name = QFileDialog::getOpenFileName(
        this, tr("载入报告模板"), {}, tr("HTML 模板 (*.html *.htm);;所有文件 (*.*)"));
    if (file_name.isEmpty()) {
        return;
    }
    QFile file(file_name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("载入失败"), file.errorString());
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    ImageReportTemplateOptions parsed;
    if (!DiagnosticReportActions::TryParseImageReportTemplateOptions(
            stream.readAll().toStdWString(), parsed)) {
        QMessageBox::warning(this, tr("无法编辑模板"),
            tr("该文件不是 CameraView 可视化模板。仍可在主窗口中将它作为自定义 HTML 模板载入。"));
        return;
    }
    applyOptions(parsed);
}

void ReportTemplateDialog::saveTemplate()
{
    QString file_name = QFileDialog::getSaveFileName(
        this, tr("保存报告模板"), QStringLiteral("CameraView-report-template.html"),
        tr("HTML 模板 (*.html)"));
    if (file_name.isEmpty()) {
        return;
    }
    if (QFileInfo(file_name).suffix().isEmpty()) {
        file_name += QStringLiteral(".html");
    }
    const ExportActionResult result = ExportActions::SaveReportTemplate(
        std::filesystem::path(file_name.toStdWString()),
        DiagnosticReportActions::BuildImageReportTemplate(options()));
    if (!result.saved) {
        QMessageBox::warning(this, tr("保存失败"), QString::fromStdWString(result.message));
    }
}
