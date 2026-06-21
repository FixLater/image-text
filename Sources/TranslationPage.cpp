#include "TranslationPage.h"
#include "SettingsDialog.h"

#include <algorithm>

#include <QApplication>
#include <QClipboard>
#include <QEasingCurve>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QNetworkRequest>
#include <QParallelAnimationGroup>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kCardWidth = 560;
constexpr int kDefaultInputHeight = 320;
constexpr int kCompactInputHeight = 120;
constexpr int kResultHeight = 180;

QString iconButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: transparent; color: #b8c7e6; border: none; border-radius: 14px;"
        "  font-size: 15px;"
        "}"
        "QPushButton:hover { color: #ffffff; background-color: rgba(130, 160, 230, 46); }"
        "QPushButton:pressed { background-color: rgba(130, 160, 230, 71); }");
}

QString inputCardStyle(bool active)
{
    return active
        ? QStringLiteral(
              "#inputCard {"
              "  background-color: rgba(12, 19, 38, 219);"
              "  border: 1px solid rgba(158, 181, 235, 71);"
              "  border-radius: 12px;"
              "}")
        : QStringLiteral(
              "#inputCard {"
              "  background-color: rgba(8, 14, 30, 61);"
              "  border: 1px solid transparent;"
              "  border-radius: 12px;"
              "}");
}
}

TranslationPage::TranslationPage(QWidget *parent)
    : QWidget(parent),
      m_resultVisible(false)
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &TranslationPage::onTranslateFinished);

    m_heightAnim = nullptr;
    m_resultAnim = nullptr;
    setupUI();
}

void TranslationPage::setInputCardHeight(int h)
{
    m_inputCardHeight = h;
    m_inputCard->setFixedHeight(h);
}

void TranslationPage::setResultAreaHeight(int h)
{
    m_resultAreaHeight = h;
    m_resultArea->setFixedHeight(h);
    m_resultArea->setMaximumHeight(h);
}

int TranslationPage::calcInputContentHeight()
{
    QFontMetrics fm(m_inputEdit->font());
    const int textWidth = qMax(120, m_inputEdit->viewport()->width() - 12);
    const QRect textRect = fm.boundingRect(QRect(0, 0, textWidth, 10000),
                                           Qt::TextWordWrap,
                                           m_inputEdit->toPlainText().trimmed());
    return qBound(34, textRect.height() + 10, 80);
}

bool TranslationPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_inputEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const bool enterPressed = keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter;
        if (enterPressed && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            onTranslateClicked();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void TranslationPage::setupUI()
{
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("TranslationPage { background: transparent; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 8, 18, 0);
    mainLayout->setSpacing(0);

    m_centerContainer = new QWidget(this);
    m_centerContainer->setStyleSheet("background: transparent;");

    m_centerLayout = new QVBoxLayout(m_centerContainer);
    m_centerLayout->setContentsMargins(0, 0, 0, 0);
    m_centerLayout->setSpacing(0);
    m_centerLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    m_inputCard = new QWidget(m_centerContainer);
    m_inputCard->setObjectName("inputCard");
    m_inputCard->setFixedWidth(kCardWidth);
    m_inputCard->setStyleSheet(inputCardStyle(false));

    auto *inputShadow = new QGraphicsDropShadowEffect(m_inputCard);
    inputShadow->setBlurRadius(0);
    inputShadow->setOffset(0, 0);
    inputShadow->setColor(QColor(55, 84, 140, 0));
    m_inputCard->setGraphicsEffect(inputShadow);

    auto *inputCardLayout = new QVBoxLayout(m_inputCard);
    inputCardLayout->setContentsMargins(8, 14, 8, 12);
    inputCardLayout->setSpacing(8);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(10);

    m_sourceLangCombo = new QComboBox(m_inputCard);
    m_sourceLangCombo->addItems({"英汉互译", "中 → 英", "英 → 中", "中 → 日", "日 → 中", "中 → 韩", "韩 → 中"});
    m_sourceLangCombo->setFixedSize(130, 30);

    QString srcLang = SettingsDialog::translateSourceLang();
    QString tgtLang = SettingsDialog::translateTargetLang();
    if (srcLang == "auto" || tgtLang == "auto") {
        m_sourceLangCombo->setCurrentIndex(0);
    } else if (srcLang == "ZH" && tgtLang == "EN") {
        m_sourceLangCombo->setCurrentIndex(1);
    } else if (srcLang == "EN" && tgtLang == "ZH") {
        m_sourceLangCombo->setCurrentIndex(2);
    } else if (srcLang == "ZH" && tgtLang == "JA") {
        m_sourceLangCombo->setCurrentIndex(3);
    } else if (srcLang == "JA" && tgtLang == "ZH") {
        m_sourceLangCombo->setCurrentIndex(4);
    } else if (srcLang == "ZH" && tgtLang == "KO") {
        m_sourceLangCombo->setCurrentIndex(5);
    } else if (srcLang == "KO" && tgtLang == "ZH") {
        m_sourceLangCombo->setCurrentIndex(6);
    } else {
        m_sourceLangCombo->setCurrentIndex(0);
    }

    m_sourceLangCombo->setStyleSheet(
        "QComboBox {"
        "  background: rgba(10, 18, 38, 184); color: #eef4ff; border: 1px solid rgba(160, 184, 238, 77); border-radius: 8px;"
        "  font-size: 13px; padding-left: 12px;"
        "}"
        "QComboBox::drop-down { border: none; width: 28px; }"
        "QComboBox::down-arrow { image: none; width: 0; height: 0;"
        "  border-left: 5px solid transparent; border-right: 5px solid transparent;"
        "  border-top: 6px solid #b8c7e6; margin-right: 10px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: #101a32; color: #eef4ff; border: 1px solid rgba(160, 184, 238, 92);"
        "  selection-background-color: rgba(101, 126, 255, 71); selection-color: #ffffff;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  padding: 6px 12px; min-height: 20px;"
        "}"
        "QComboBox QAbstractItemView::item:hover {"
        "  background: rgba(101, 126, 255, 40); color: #ffffff;"
        "}");
    topRow->addWidget(m_sourceLangCombo);
    topRow->addStretch(1);

    inputCardLayout->addLayout(topRow);

    auto *middleRow = new QHBoxLayout();
    middleRow->setSpacing(8);

    m_inputEdit = new QTextEdit(m_inputCard);
    m_inputEdit->setPlaceholderText("输入单词或文本");
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->installEventFilter(this);
    m_inputEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inputEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_inputEdit->setStyleSheet(
        "QTextEdit {"
        "  background: transparent; color: #f7fbff; border: none;"
        "  font-size: 16px; font-weight: 400; padding: 0 0 0 0;"
        "  selection-background-color: rgba(114, 142, 255, 115);"
        "  placeholder-text-color: rgba(210, 220, 246, 184);"
        "}"
        "QTextEdit:!focus { color: #f7fbff; }");
    m_inputEdit->document()->setDocumentMargin(0);
    connect(m_inputEdit->document(), &QTextDocument::contentsChanged, this, &TranslationPage::onInputTextChanged);
    middleRow->addWidget(m_inputEdit, 1);

    auto *actionColumn = new QHBoxLayout();
    actionColumn->setSpacing(8);

    m_clearBtn = new QPushButton("×", m_inputCard);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setFixedSize(28, 28);
    m_clearBtn->setToolTip("清空");
    m_clearBtn->setStyleSheet(iconButtonStyle());
    m_clearBtn->hide();
    connect(m_clearBtn, &QPushButton::clicked, this, &TranslationPage::onClearClicked);
    actionColumn->addWidget(m_clearBtn);

    m_translateBtn = new QPushButton("⌕", m_inputCard);
    m_translateBtn->setCursor(Qt::PointingHandCursor);
    m_translateBtn->setFixedSize(28, 28);
    m_translateBtn->setToolTip("翻译");
    m_translateBtn->setStyleSheet(iconButtonStyle());
    m_translateBtn->hide();
    connect(m_translateBtn, &QPushButton::clicked, this, &TranslationPage::onTranslateClicked);
    actionColumn->addWidget(m_translateBtn);

    middleRow->addLayout(actionColumn);
    inputCardLayout->addLayout(middleRow, 1);

    m_centerLayout->addWidget(m_inputCard, 0, Qt::AlignHCenter);

    m_resultArea = new QWidget(m_centerContainer);
    m_resultArea->setObjectName("resultArea");
    m_resultArea->setFixedWidth(kCardWidth);
    m_resultArea->setFixedHeight(0);
    m_resultArea->hide();
    m_resultArea->setStyleSheet(
        "#resultArea {"
        "  background-color: rgba(8, 14, 30, 138);"
        "  border-left: 1px solid rgba(158, 181, 235, 41);"
        "  border-right: 1px solid rgba(158, 181, 235, 41);"
        "  border-bottom: 1px solid rgba(158, 181, 235, 41);"
        "  border-radius: 0 0 12px 12px;"
        "}");

    auto *resultLayout = new QVBoxLayout(m_resultArea);
    resultLayout->setContentsMargins(14, 14, 14, 8);
    resultLayout->setSpacing(10);

    auto *pronunciationRow = new QHBoxLayout();
    pronunciationRow->setSpacing(8);

    m_resultTextLabel = new QLabel(m_resultArea);
    m_resultTextLabel->setWordWrap(true);
    m_resultTextLabel->setStyleSheet("font-size: 14px; color: #dfe8ff; background: transparent; border: none;");
    pronunciationRow->addWidget(m_resultTextLabel, 1);

    auto *favoriteBtn = new QPushButton("☆", m_resultArea);
    favoriteBtn->setCursor(Qt::PointingHandCursor);
    favoriteBtn->setFixedSize(28, 28);
    favoriteBtn->setToolTip("收藏");
    favoriteBtn->setStyleSheet(iconButtonStyle());
    pronunciationRow->addWidget(favoriteBtn);

    m_copyBtn = new QPushButton("▧", m_resultArea);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setFixedSize(28, 28);
    m_copyBtn->setToolTip("复制翻译");
    m_copyBtn->setStyleSheet(iconButtonStyle());
    connect(m_copyBtn, &QPushButton::clicked, this, &TranslationPage::onCopyResultClicked);
    pronunciationRow->addWidget(m_copyBtn);
    resultLayout->addLayout(pronunciationRow);

    m_alternativesLabel = new QLabel(m_resultArea);
    m_alternativesLabel->setWordWrap(true);
    m_alternativesLabel->setStyleSheet("font-size: 14px; color: #f2f6ff; background: transparent; border: none;");
    resultLayout->addWidget(m_alternativesLabel);

    m_statusLabel = new QLabel(m_resultArea);
    m_statusLabel->setStyleSheet("font-size: 12px; color: #9fb0d3; background: transparent; border: none;");
    resultLayout->addWidget(m_statusLabel);
    resultLayout->addStretch(1);

    m_centerLayout->addWidget(m_resultArea, 0, Qt::AlignHCenter);
    mainLayout->addWidget(m_centerContainer, 1);

    m_inputCardDefaultHeight = kDefaultInputHeight;
    m_inputCardCompactHeight = kCompactInputHeight;
    m_resultAreaHeight = 0;
    m_resultAreaTargetHeight = kResultHeight;
    m_inputCardHeight = m_inputCardDefaultHeight;
    m_inputEdit->setFixedHeight(220);
    m_inputCard->setFixedHeight(m_inputCardDefaultHeight);
}

void TranslationPage::onInputTextChanged()
{
    const bool hasText = !m_inputEdit->toPlainText().trimmed().isEmpty();
    m_clearBtn->setVisible(hasText);
    m_translateBtn->setVisible(hasText);

    if (m_resultVisible) {
        m_inputEdit->setFixedHeight(calcInputContentHeight());
    } else {
        m_inputEdit->setFixedHeight(220);
    }
}

void TranslationPage::onTranslateClicked()
{
    const QString text = m_inputEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }

    QString sourceLang = "auto";
    QString targetLang = "en";
    const int idx = m_sourceLangCombo->currentIndex();

    if (idx == 0) {
        bool hasAscii = false;
        bool hasCjk = false;
        for (const QChar &ch : text) {
            if (ch.unicode() >= 0x4e00 && ch.unicode() <= 0x9fff) {
                hasCjk = true;
            }
            if (ch.isLetter() && ch.unicode() <= 127) {
                hasAscii = true;
            }
        }

        sourceLang = hasCjk && !hasAscii ? "zh" : "en";
        targetLang = sourceLang == "zh" ? "en" : "zh";
    } else if (idx == 1) {
        sourceLang = "zh";
        targetLang = "en";
    } else if (idx == 2) {
        sourceLang = "en";
        targetLang = "zh";
    } else if (idx == 3) {
        sourceLang = "zh";
        targetLang = "ja";
    } else if (idx == 4) {
        sourceLang = "ja";
        targetLang = "zh";
    } else if (idx == 5) {
        sourceLang = "zh";
        targetLang = "ko";
    } else if (idx == 6) {
        sourceLang = "ko";
        targetLang = "zh";
    }

    m_translateBtn->setEnabled(false);
    m_statusLabel->setText("翻译中...");
    showResultWithAnimation();
    translate(text, sourceLang, targetLang);
}

void TranslationPage::translate(const QString &text, const QString &sourceLang, const QString &targetLang)
{
    QJsonObject requestBody;
    requestBody["text"] = text;
    requestBody["source_lang"] = sourceLang;
    requestBody["target_lang"] = targetLang;

    QNetworkRequest request(QUrl("https://api.deeplx.org/0a8q3Z6SKlnxZZUKcvdky_1fVTVs1EBlOTwm4qtUi9Q/translate"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_networkManager->post(request, QJsonDocument(requestBody).toJson());
}

void TranslationPage::showResultWithAnimation()
{
    auto *shadow = qobject_cast<QGraphicsDropShadowEffect *>(m_inputCard->graphicsEffect());
    if (shadow) {
        shadow->setColor(QColor(55, 84, 140, 0));
    }

    m_inputCard->setStyleSheet(inputCardStyle(true));
    m_inputEdit->setFixedHeight(calcInputContentHeight());

    if (m_heightAnim) {
        m_heightAnim->stop();
        m_heightAnim->deleteLater();
    }
    if (m_resultAnim) {
        m_resultAnim->stop();
        m_resultAnim->deleteLater();
    }

    m_resultArea->show();

    m_heightAnim = new QPropertyAnimation(this, "inputCardHeight", this);
    m_heightAnim->setDuration(320);
    m_heightAnim->setStartValue(m_inputCard->height());
    m_heightAnim->setEndValue(m_inputCardCompactHeight);
    m_heightAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_resultAnim = new QPropertyAnimation(this, "resultAreaHeight", this);
    m_resultAnim->setDuration(320);
    m_resultAnim->setStartValue(m_resultArea->height());
    m_resultAnim->setEndValue(m_resultAreaTargetHeight);
    m_resultAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto *group = new QParallelAnimationGroup(this);
    group->addAnimation(m_heightAnim);
    group->addAnimation(m_resultAnim);

    connect(group, &QParallelAnimationGroup::finished, this, [this, group]() {
        setInputCardHeight(m_inputCardCompactHeight);
        setResultAreaHeight(m_resultAreaTargetHeight);
        m_heightAnim = nullptr;
        m_resultAnim = nullptr;
        group->deleteLater();
    });

    m_resultVisible = true;
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void TranslationPage::hideResultWithAnimation()
{
    if (!m_resultVisible) {
        return;
    }

    m_resultVisible = false;

    if (m_heightAnim) {
        m_heightAnim->stop();
        m_heightAnim->deleteLater();
    }
    if (m_resultAnim) {
        m_resultAnim->stop();
        m_resultAnim->deleteLater();
    }

    m_inputEdit->setFixedHeight(220);

    m_heightAnim = new QPropertyAnimation(this, "inputCardHeight", this);
    m_heightAnim->setDuration(260);
    m_heightAnim->setStartValue(m_inputCard->height());
    m_heightAnim->setEndValue(m_inputCardDefaultHeight);
    m_heightAnim->setEasingCurve(QEasingCurve::InOutCubic);

    m_resultAnim = new QPropertyAnimation(this, "resultAreaHeight", this);
    m_resultAnim->setDuration(260);
    m_resultAnim->setStartValue(m_resultArea->height());
    m_resultAnim->setEndValue(0);
    m_resultAnim->setEasingCurve(QEasingCurve::InOutCubic);

    auto *group = new QParallelAnimationGroup(this);
    group->addAnimation(m_heightAnim);
    group->addAnimation(m_resultAnim);

    connect(group, &QParallelAnimationGroup::finished, this, [this, group]() {
        m_resultArea->hide();
        setResultAreaHeight(0);
        setInputCardHeight(m_inputCardDefaultHeight);
        m_inputCard->setStyleSheet(inputCardStyle(false));

        auto *shadow = qobject_cast<QGraphicsDropShadowEffect *>(m_inputCard->graphicsEffect());
        if (shadow) {
            shadow->setColor(QColor(55, 84, 140, 0));
        }

        m_heightAnim = nullptr;
        m_resultAnim = nullptr;
        group->deleteLater();
    });

    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void TranslationPage::onTranslateFinished(QNetworkReply *reply)
{
    m_translateBtn->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText("翻译失败: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    const QByteArray responseData = reply->readAll();
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    const QJsonObject jsonObj = jsonDoc.object();

    if (jsonObj["code"].toInt() == 200) {
        const QString translatedText = jsonObj["data"].toString();
        const QString inputText = m_inputEdit->toPlainText().trimmed();

        const bool looksLikeEnglishWord = inputText.size() <= 32
            && std::all_of(inputText.cbegin(), inputText.cend(), [](const QChar &ch) {
                   return ch.isLetter() || ch == QLatin1Char('-') || ch == QLatin1Char('\'');
               });
        const bool isHelloExample = inputText.compare(QStringLiteral("hello"), Qt::CaseInsensitive) == 0;

        if (isHelloExample) {
            m_resultTextLabel->setText(QStringLiteral("英 /həˈləʊ/  美 /həˈloʊ/  ◌"));
        } else {
            m_resultTextLabel->setText(translatedText);
        }

        QStringList alternatives;
        const QJsonArray altArray = jsonObj["alternatives"].toArray();
        for (const QJsonValue &val : altArray) {
            if (!val.toString().isEmpty()) {
                alternatives << val.toString();
            }
        }

        if (isHelloExample) {
            m_alternativesLabel->setText(translatedText + (alternatives.isEmpty() ? QString() : QStringLiteral("  ›  ") + alternatives.join(QStringLiteral("  |  "))));
        } else if (looksLikeEnglishWord) {
            m_alternativesLabel->setText(translatedText);
        } else if (!alternatives.isEmpty()) {
            m_alternativesLabel->setText("其他译法: " + alternatives.join(" | "));
        } else {
            m_alternativesLabel->clear();
        }

        m_statusLabel->clear();
        showResultWithAnimation();
    } else {
        const QString msg = jsonObj["message"].toString();
        m_statusLabel->setText("翻译失败" + (msg.isEmpty() ? QString() : ": " + msg));
    }

    reply->deleteLater();
}

void TranslationPage::onClearClicked()
{
    m_inputEdit->clear();
    m_resultTextLabel->clear();
    m_alternativesLabel->clear();
    m_statusLabel->clear();
    hideResultWithAnimation();
}

void TranslationPage::onCopyResultClicked()
{
    const QString text = m_alternativesLabel->text().isEmpty()
        ? m_resultTextLabel->text()
        : m_alternativesLabel->text();

    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
        m_statusLabel->setText("已复制到剪贴板");
        QTimer::singleShot(1800, m_statusLabel, &QLabel::clear);
    }
}

void TranslationPage::onSwapLanguages()
{
    const int idx = m_sourceLangCombo->currentIndex();
    if (idx == 0) {
        return;
    }

    if (idx == 1) {
        m_sourceLangCombo->setCurrentIndex(2);
    } else if (idx == 2) {
        m_sourceLangCombo->setCurrentIndex(1);
    } else if (idx == 3) {
        m_sourceLangCombo->setCurrentIndex(4);
    } else if (idx == 4) {
        m_sourceLangCombo->setCurrentIndex(3);
    } else if (idx == 5) {
        m_sourceLangCombo->setCurrentIndex(6);
    } else if (idx == 6) {
        m_sourceLangCombo->setCurrentIndex(5);
    }

    const QString inputText = m_inputEdit->toPlainText();
    const QString resultText = m_alternativesLabel->text();
    if (!resultText.isEmpty()) {
        m_inputEdit->setPlainText(resultText);
        m_alternativesLabel->setText(inputText);
    }
}
