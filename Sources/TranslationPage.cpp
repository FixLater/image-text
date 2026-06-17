#include "TranslationPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QClipboard>
#include <QApplication>
#include <QPropertyAnimation>
#include <QTimer>
#include <QScrollBar>

TranslationPage::TranslationPage(QWidget *parent) : QWidget(parent), m_resultVisible(false) {
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &TranslationPage::onTranslateFinished);
    m_heightAnim = nullptr;
    setupUI();
}

void TranslationPage::setInputCardHeight(int h) {
    m_inputCardHeight = h;
    m_inputCard->setFixedHeight(h);
}

int TranslationPage::calcInputContentHeight() {
    QFontMetrics fm(m_inputEdit->font());
    int textWidth = m_inputEdit->viewport()->width() - 20;
    if (textWidth <= 0) textWidth = m_inputEdit->width() - 40;
    QRect textRect = fm.boundingRect(QRect(0, 0, textWidth, 10000),
                                     Qt::TextWordWrap, m_inputEdit->toPlainText());
    int lines = qMax(1, (textRect.height() + fm.height() - 1) / fm.height());
    int contentH = lines * fm.height() + 20;
    return qMax(80, qMin(contentH, 300));
}

void TranslationPage::setupUI() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background: transparent;");
    auto *headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(32, 24, 32, 0);
    headerLayout->setSpacing(0);

    auto *titleLabel = new QLabel("翻译");
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    titleLabel->setStyleSheet(
        "font-size: 20pt; font-weight: bold; color: #e2e8f0; background: transparent; border: none; padding-bottom: 4px;"
    );
    headerLayout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel("使用 DeepL 进行多语言文本翻译");
    subtitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    subtitleLabel->setStyleSheet(
        "font-size: 9pt; color: #64748b; background: transparent; border: none; padding-bottom: 20px;"
    );
    headerLayout->addWidget(subtitleLabel);

    mainLayout->addWidget(headerWidget);

    m_centerContainer = new QWidget(this);
    m_centerContainer->setStyleSheet("background: transparent;");
    m_centerLayout = new QVBoxLayout(m_centerContainer);
    m_centerLayout->setContentsMargins(32, 0, 32, 0);
    m_centerLayout->setSpacing(0);
    m_centerLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    m_inputCard = new QWidget();
    m_inputCard->setObjectName("inputCard");
    m_inputCard->setFixedWidth(560);
    m_inputCard->setStyleSheet(
        "#inputCard {"
        "  background-color: #1e293b; border: 1px solid #334155; border-radius: 12px;"
        "}"
    );
    auto *inputShadow = new QGraphicsDropShadowEffect();
    inputShadow->setBlurRadius(24);
    inputShadow->setOffset(0, 4);
    inputShadow->setColor(QColor(0, 0, 0, 60));
    m_inputCard->setGraphicsEffect(inputShadow);

    auto *inputCardLayout = new QVBoxLayout(m_inputCard);
    inputCardLayout->setContentsMargins(20, 16, 20, 16);
    inputCardLayout->setSpacing(0);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(0);

    m_sourceLangCombo = new QComboBox();
    m_sourceLangCombo->addItems({"英汉互译", "中→英", "英→中", "中→日", "日→中", "中→韩", "韩→中"});
    m_sourceLangCombo->setFixedHeight(32);
    m_sourceLangCombo->setStyleSheet(
        "QComboBox {"
        "  background: transparent; color: #e2e8f0; border: none;"
        "  font-size: 9pt; padding: 0 4px;"
        "}"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox::down-arrow { image: none; border-left: 3px solid transparent; border-right: 3px solid transparent;"
        "  border-top: 5px solid #64748b; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #1e293b; color: #e2e8f0; border: 1px solid #334155;"
        "  selection-background-color: rgba(14, 165, 233, 0.2); selection-color: #0ea5e9;"
        "}"
    );
    topRow->addWidget(m_sourceLangCombo);
    topRow->addStretch(1);

    m_translateBtn = new QPushButton("搜索");
    m_translateBtn->setCursor(Qt::PointingHandCursor);
    m_translateBtn->setFixedSize(32, 32);
    m_translateBtn->setStyleSheet(
        "QPushButton {"
        "  background: transparent; color: #64748b; border: none; border-radius: 16px;"
        "  font-size: 14pt;"
        "}"
        "QPushButton:hover { color: #0ea5e9; background-color: rgba(14, 165, 233, 0.1); }"
    );
    connect(m_translateBtn, &QPushButton::clicked, this, &TranslationPage::onTranslateClicked);
    topRow->addWidget(m_translateBtn);

    inputCardLayout->addLayout(topRow);

    auto *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);
    separator->setStyleSheet("background-color: #334155;");
    inputCardLayout->addWidget(separator);

    m_inputEdit = new QTextEdit();
    m_inputEdit->setPlaceholderText("输入单词或文本");
    m_inputEdit->setStyleSheet(
        "QTextEdit {"
        "  background: transparent; color: #e2e8f0; border: none;"
        "  font-size: 14pt; padding: 4px 0; selection-background-color: rgba(14, 165, 233, 0.3);"
        "}"
    );
    m_inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_inputEdit->document(), &QTextDocument::contentsChanged, this, &TranslationPage::onInputTextChanged);
    inputCardLayout->addWidget(m_inputEdit);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(0);
    bottomRow->addStretch(1);

    m_clearBtn = new QPushButton();
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setFixedSize(28, 28);
    m_clearBtn->setToolTip("清空");
    m_clearBtn->setStyleSheet(
        "QPushButton {"
        "  background: transparent; color: #64748b; border: none; border-radius: 14px;"
        "  font-size: 12pt;"
        "}"
        "QPushButton:hover { color: #e2e8f0; background-color: rgba(100, 116, 139, 0.2); }"
    );
    connect(m_clearBtn, &QPushButton::clicked, this, &TranslationPage::onClearClicked);
    bottomRow->addWidget(m_clearBtn);

    inputCardLayout->addLayout(bottomRow);

    m_centerLayout->addWidget(m_inputCard, 0, Qt::AlignHCenter);

    m_resultArea = new QWidget();
    m_resultArea->setObjectName("resultArea");
    m_resultArea->setFixedWidth(560);
    m_resultArea->setMaximumHeight(0);
    m_resultArea->setMinimumHeight(0);
    m_resultArea->setStyleSheet(
        "#resultArea {"
        "  background-color: #1e293b; border: 1px solid #334155; border-top: none;"
        "  border-radius: 0 0 12px 12px;"
        "}"
    );

    auto *resultLayout = new QVBoxLayout(m_resultArea);
    resultLayout->setContentsMargins(20, 16, 20, 16);
    resultLayout->setSpacing(8);

    m_resultTextLabel = new QLabel();
    m_resultTextLabel->setWordWrap(true);
    m_resultTextLabel->setStyleSheet(
        "font-size: 14pt; color: #e2e8f0; background: transparent; border: none;"
    );
    resultLayout->addWidget(m_resultTextLabel);

    auto *altSeparator = new QFrame();
    altSeparator->setFrameShape(QFrame::HLine);
    altSeparator->setFixedHeight(1);
    altSeparator->setStyleSheet("background-color: #334155;");
    resultLayout->addWidget(altSeparator);

    auto *bottomArea = new QHBoxLayout();
    bottomArea->setSpacing(8);

    m_alternativesLabel = new QLabel();
    m_alternativesLabel->setWordWrap(true);
    m_alternativesLabel->setStyleSheet(
        "font-size: 9pt; color: #94a3b8; background: transparent; border: none;"
    );
    bottomArea->addWidget(m_alternativesLabel, 1);

    m_copyBtn = new QPushButton();
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setFixedSize(28, 28);
    m_copyBtn->setToolTip("复制翻译");
    m_copyBtn->setStyleSheet(
        "QPushButton {"
        "  background: transparent; color: #64748b; border: none; border-radius: 14px;"
        "  font-size: 11pt;"
        "}"
        "QPushButton:hover { color: #0ea5e9; background-color: rgba(14, 165, 233, 0.1); }"
    );
    connect(m_copyBtn, &QPushButton::clicked, this, &TranslationPage::onCopyResultClicked);
    bottomArea->addWidget(m_copyBtn);

    resultLayout->addLayout(bottomArea);

    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("font-size: 8pt; color: #64748b; background: transparent; border: none;");
    resultLayout->addWidget(m_statusLabel);

    m_centerLayout->addWidget(m_resultArea, 0, Qt::AlignHCenter);

    mainLayout->addWidget(m_centerContainer, 1);

    m_inputCardDefaultHeight = 280;
    m_inputCardHeight = m_inputCardDefaultHeight;
    m_inputCard->setFixedHeight(m_inputCardDefaultHeight);
}

void TranslationPage::onInputTextChanged() {
    QTextDocument *doc = m_inputEdit->document();
    doc->adjustSize();
    int h = doc->size().toSize().height() + 8;
    h = qMax(60, qMin(h, 300));
    m_inputEdit->setFixedHeight(h);
}

void TranslationPage::onTranslateClicked() {
    QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    QString langPair = m_sourceLangCombo->currentText();
    QString sourceLang = "auto";
    QString targetLang = "en";

    if (langPair == "英汉互译") {
        bool hasAscii = false;
        bool hasCjk = false;
        for (const QChar &ch : text) {
            if (ch.unicode() > 0x2000) hasCjk = true;
            if (ch.isLetter() && ch.unicode() <= 127) hasAscii = true;
        }
        if (hasCjk && !hasAscii) {
            sourceLang = "zh";
            targetLang = "en";
        } else {
            sourceLang = "en";
            targetLang = "zh";
        }
    } else if (langPair == "中→英") { sourceLang = "zh"; targetLang = "en"; }
    else if (langPair == "英→中") { sourceLang = "en"; targetLang = "zh"; }
    else if (langPair == "中→日") { sourceLang = "zh"; targetLang = "ja"; }
    else if (langPair == "日→中") { sourceLang = "ja"; targetLang = "zh"; }
    else if (langPair == "中→韩") { sourceLang = "zh"; targetLang = "ko"; }
    else if (langPair == "韩→中") { sourceLang = "ko"; targetLang = "zh"; }

    m_translateBtn->setEnabled(false);
    m_statusLabel->setText("翻译中...");

    translate(text, sourceLang, targetLang);
}

void TranslationPage::translate(const QString &text, const QString &sourceLang, const QString &targetLang) {
    QJsonObject requestBody;
    requestBody["text"] = text;
    requestBody["source_lang"] = sourceLang;
    requestBody["target_lang"] = targetLang;

    QNetworkRequest request(QUrl("https://api.deeplx.org/0a8q3Z6SKlnxZZUKcvdky_1fVTVs1EBlOTwm4qtUi9Q/translate"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_networkManager->post(request, QJsonDocument(requestBody).toJson());
}

void TranslationPage::showResultWithAnimation() {
    int targetInputH = calcInputContentHeight();

    if (m_heightAnim) {
        m_heightAnim->stop();
        m_heightAnim->deleteLater();
    }

    m_resultArea->setMaximumHeight(0);
    m_resultArea->show();

    m_heightAnim = new QPropertyAnimation(this, "inputCardHeight", this);
    m_heightAnim->setDuration(350);
    m_heightAnim->setStartValue(m_inputCard->height());
    m_heightAnim->setEndValue(targetInputH);
    m_heightAnim->setEasingCurve(QEasingCurve::OutCubic);

    QPropertyAnimation *resultAnim = new QPropertyAnimation(m_resultArea, "maximumHeight", this);
    resultAnim->setDuration(350);
    resultAnim->setStartValue(0);
    resultAnim->setEndValue(160);
    resultAnim->setEasingCurve(QEasingCurve::OutCubic);

    QParallelAnimationGroup *group = new QParallelAnimationGroup(this);
    group->addAnimation(m_heightAnim);
    group->addAnimation(resultAnim);

    connect(group, &QParallelAnimationGroup::finished, this, [this, group]() {
        m_resultArea->setMaximumHeight(16777215);
        m_inputCard->setFixedHeight(m_inputCard->minimumSizeHint().height());
        m_heightAnim = nullptr;
        group->deleteLater();
    });

    m_resultVisible = true;
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void TranslationPage::hideResultWithAnimation() {
    if (!m_resultVisible) return;
    m_resultVisible = false;

    if (m_heightAnim) {
        m_heightAnim->stop();
        m_heightAnim->deleteLater();
    }

    int currentInputH = m_inputCard->height();

    m_heightAnim = new QPropertyAnimation(this, "inputCardHeight", this);
    m_heightAnim->setDuration(300);
    m_heightAnim->setStartValue(currentInputH);
    m_heightAnim->setEndValue(m_inputCardDefaultHeight);
    m_heightAnim->setEasingCurve(QEasingCurve::InCubic);

    QPropertyAnimation *resultAnim = new QPropertyAnimation(m_resultArea, "maximumHeight", this);
    resultAnim->setDuration(300);
    resultAnim->setStartValue(m_resultArea->height());
    resultAnim->setEndValue(0);
    resultAnim->setEasingCurve(QEasingCurve::InCubic);

    QParallelAnimationGroup *group = new QParallelAnimationGroup(this);
    group->addAnimation(m_heightAnim);
    group->addAnimation(resultAnim);

    connect(group, &QParallelAnimationGroup::finished, this, [this, group]() {
        m_resultArea->hide();
        m_resultArea->setMaximumHeight(0);
        m_heightAnim = nullptr;
        group->deleteLater();
    });

    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void TranslationPage::onTranslateFinished(QNetworkReply *reply) {
    m_translateBtn->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText("翻译失败: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    QJsonObject jsonObj = jsonDoc.object();

    int code = jsonObj["code"].toInt();
    if (code == 200) {
        QString translatedText = jsonObj["data"].toString();
        m_resultTextLabel->setText(translatedText);

        QStringList alternatives;
        QJsonArray altArray = jsonObj["alternatives"].toArray();
        for (const QJsonValue &val : altArray) {
            alternatives << val.toString();
        }

        if (!alternatives.isEmpty()) {
            m_alternativesLabel->setText("其他译法: " + alternatives.join(" | "));
            m_alternativesLabel->show();
        } else {
            m_alternativesLabel->hide();
        }

        m_statusLabel->clear();
        showResultWithAnimation();
    } else {
        QString msg = jsonObj["message"].toString();
        m_statusLabel->setText("翻译失败" + (msg.isEmpty() ? "" : ": " + msg));
    }

    reply->deleteLater();
}

void TranslationPage::onClearClicked() {
    m_inputEdit->clear();
    m_resultTextLabel->clear();
    m_alternativesLabel->clear();
    m_statusLabel->clear();
    hideResultWithAnimation();
}

void TranslationPage::onCopyResultClicked() {
    QString text = m_resultTextLabel->text();
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
        m_statusLabel->setText("已复制到剪贴板");
        QTimer::singleShot(2000, m_statusLabel, &QLabel::clear);
    }
}

void TranslationPage::onSwapLanguages() {
    QString langPair = m_sourceLangCombo->currentText();
    int idx = m_sourceLangCombo->currentIndex();

    if (idx == 0) return;

    if (langPair == "中→英") m_sourceLangCombo->setCurrentIndex(2);
    else if (langPair == "英→中") m_sourceLangCombo->setCurrentIndex(1);
    else if (langPair == "中→日") m_sourceLangCombo->setCurrentIndex(4);
    else if (langPair == "日→中") m_sourceLangCombo->setCurrentIndex(3);
    else if (langPair == "中→韩") m_sourceLangCombo->setCurrentIndex(6);
    else if (langPair == "韩→中") m_sourceLangCombo->setCurrentIndex(5);

    QString inputText = m_inputEdit->toPlainText();
    QString resultText = m_resultTextLabel->text();
    if (!resultText.isEmpty()) {
        m_inputEdit->setPlainText(resultText);
        m_resultTextLabel->setText(inputText);
    }
}
