#ifndef TRANSLATIONPAGE_H
#define TRANSLATIONPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

class TranslationPage : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int inputCardHeight READ inputCardHeight WRITE setInputCardHeight)
    Q_PROPERTY(int resultAreaHeight READ resultAreaHeight WRITE setResultAreaHeight)

public:
    explicit TranslationPage(QWidget *parent = nullptr);
    int inputCardHeight() const { return m_inputCardHeight; }
    void setInputCardHeight(int h);
    int resultAreaHeight() const { return m_resultAreaHeight; }
    void setResultAreaHeight(int h);

private slots:
    void onTranslateClicked();
    void onClearClicked();
    void onCopyResultClicked();
    void onSwapLanguages();
    void onTranslateFinished(QNetworkReply *reply);
    void onInputTextChanged();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setupUI();
    void translate(const QString &text, const QString &sourceLang, const QString &targetLang);
    void showResultWithAnimation();
    void hideResultWithAnimation();
    int calcInputContentHeight();

    QWidget *m_centerContainer;
    QVBoxLayout *m_centerLayout;
    QWidget *m_inputCard;
    QTextEdit *m_inputEdit;
    QWidget *m_resultArea;
    QLabel *m_resultTextLabel;
    QLabel *m_alternativesLabel;
    QComboBox *m_sourceLangCombo;
    QPushButton *m_translateBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_copyBtn;
    QPushButton *m_swapBtn;
    QLabel *m_statusLabel;
    QNetworkAccessManager *m_networkManager;

    int m_inputCardHeight;
    int m_inputCardDefaultHeight;
    int m_inputCardCompactHeight;
    int m_resultAreaHeight;
    int m_resultAreaTargetHeight;
    bool m_resultVisible;
    QPropertyAnimation *m_heightAnim;
    QPropertyAnimation *m_resultAnim;
};

#endif // TRANSLATIONPAGE_H
