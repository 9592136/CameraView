#pragma once

#include "app/DiagnosticReportActions.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTextBrowser;

class ReportTemplateDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ReportTemplateDialog(
        const ImageReportTemplateOptions& options,
        QWidget* parent = nullptr);

    ImageReportTemplateOptions options() const;

private:
    void applyOptions(const ImageReportTemplateOptions& options);
    void refreshSectionEditor();
    void updatePreview();
    void moveCurrentSection(int delta);
    void loadTemplate();
    void saveTemplate();

    QLineEdit* title_edit_ = nullptr;
    QLineEdit* subtitle_edit_ = nullptr;
    QComboBox* accent_combo_ = nullptr;
    QComboBox* image_size_combo_ = nullptr;
    QComboBox* page_layout_combo_ = nullptr;
    QComboBox* orientation_combo_ = nullptr;
    QComboBox* precision_combo_ = nullptr;
    QLineEdit* image_caption_edit_ = nullptr;
    QLineEdit* footer_edit_ = nullptr;
    QCheckBox* raw_values_check_ = nullptr;
    QCheckBox* group_measurements_check_ = nullptr;
    QCheckBox* calibration_check_ = nullptr;
    QCheckBox* processing_check_ = nullptr;
    QCheckBox* footer_check_ = nullptr;
    QPlainTextEdit* information_edit_ = nullptr;
    QPlainTextEdit* notes_edit_ = nullptr;
    QListWidget* section_list_ = nullptr;
    QLineEdit* section_heading_edit_ = nullptr;
    QTextBrowser* preview_ = nullptr;
};
