#include "pages/ai_page.h"

#include "widgets/status_badge.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace cockpit::ui {

AiPage::AiPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 14, 10);
    root->setSpacing(8);

    auto* heading = new QLabel(QStringLiteral("AI · Vision + Voice · MOCK"), this);
    heading->setStyleSheet("font-size: 19px; font-weight: 700;");
    root->addWidget(heading);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(10);

    auto* vision_box = new QGroupBox(QStringLiteral("Vision"), this);
    auto* vision_layout = new QVBoxLayout(vision_box);
    vision_status_ = new StatusBadge(QStringLiteral("Service"), vision_box);
    vision_details_ = new QLabel(vision_box);
    vision_details_->setWordWrap(true);
    vision_layout->addWidget(vision_status_);
    vision_layout->addWidget(vision_details_, 1);
    columns->addWidget(vision_box, 1);

    auto* voice_box = new QGroupBox(QStringLiteral("Voice"), this);
    auto* voice_layout = new QVBoxLayout(voice_box);
    voice_status_ = new StatusBadge(QStringLiteral("Service"), voice_box);
    llm_status_ = new StatusBadge(QStringLiteral("LLM"), voice_box);
    voice_details_ = new QLabel(voice_box);
    voice_details_->setWordWrap(true);
    session_button_ = new QPushButton(QStringLiteral("Start Voice Session"), voice_box);
    session_button_->setObjectName(QStringLiteral("voice_session"));
    session_button_->setMinimumHeight(48);
    voice_layout->addWidget(voice_status_);
    voice_layout->addWidget(llm_status_);
    voice_layout->addWidget(voice_details_, 1);
    voice_layout->addWidget(session_button_);
    columns->addWidget(voice_box, 1);
    root->addLayout(columns, 1);

    result_ = new QLabel(QStringLiteral("Latest RESULT: N/A · MOCK backend"), this);
    result_->setObjectName(QStringLiteral("ai_result"));
    result_->setMinimumHeight(30);
    result_->setWordWrap(true);
    root->addWidget(result_);

    connect(session_button_, &QPushButton::clicked, this,
            [this] { emit voiceSessionRequested(!session_active_); });
}

void AiPage::setState(const UiState& state) {
    vision_status_->setStatus(state.vision);
    voice_status_->setStatus(state.voice);
    llm_status_->setStatus(state.language_model);
    vision_details_->setText(
        QStringLiteral("Model: %1\nCamera: %2\nLatest inference: %3\nInference rate: %4")
            .arg(QString::fromStdString(state.vision_model),
                 QString::fromStdString(state.current_camera),
                 QString::fromStdString(state.latest_inference),
                 QString::fromStdString(state.inference_rate)));
    voice_details_->setText(
        QStringLiteral("Session: %1\nASR: %2\nTranscript: %3\nIntent: %4\nTTS: %5")
            .arg(QString::fromStdString(state.voice_session),
                 QString::fromStdString(state.asr_state),
                 QString::fromStdString(state.latest_asr_text),
                 QString::fromStdString(state.intent_state),
                 QString::fromStdString(state.tts_state)));
    result_->setText(QStringLiteral("Latest RESULT: %1")
                         .arg(QString::fromStdString(state.latest_result)));
    session_active_ = state.voice_session == "Active" || state.voice_session == "Starting";
    session_button_->setText(session_active_ ? QStringLiteral("Cancel Voice Session")
                                              : QStringLiteral("Start Voice Session"));
    session_button_->setEnabled(!state.voice_pending);
}

void AiPage::showResult(const UiResult& result) {
    result_->setText(QStringLiteral("RESULT #%1: %2")
                         .arg(static_cast<qulonglong>(result.request_id))
                         .arg(QString::fromStdString(result.message)));
    result_->setStyleSheet(result.succeeded() ? "color: #8dd9b0;" : "color: #ff9aa6;");
}

}  // namespace cockpit::ui
