#include "StarBackground.h"
#include <QPainterPath>

StarBackground::StarBackground(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);

    initStars();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &StarBackground::updateAnimation);
    m_timer->start(33);
}

void StarBackground::initStars() {
    m_stars.clear();
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < m_maxStars; ++i) {
        Star s;
        s.x = rng->generateDouble();
        s.y = rng->generateDouble();
        s.radius = 0.5 + rng->generateDouble() * 1.5;
        s.opacity = 0.3 + rng->generateDouble() * 0.7;
        s.twinkleSpeed = 0.5 + rng->generateDouble() * 2.0;
        s.twinklePhase = rng->generateDouble() * M_PI * 2.0;
        m_stars.append(s);
    }
}

void StarBackground::spawnMeteor() {
    auto *rng = QRandomGenerator::global();
    Meteor m;
    m.x = rng->generateDouble() * 0.8;
    m.y = rng->generateDouble() * 0.3;
    m.length = 0.05 + rng->generateDouble() * 0.08;
    m.speed = 0.003 + rng->generateDouble() * 0.004;
    m.angle = M_PI / 4.0 + (rng->generateDouble() - 0.5) * 0.3;
    m.opacity = 0.6 + rng->generateDouble() * 0.4;
    m.life = 0.0;
    m.maxLife = 0.6 + rng->generateDouble() * 0.6;
    m_meteors.append(m);
}

void StarBackground::updateAnimation() {
    m_time += 0.033;

    if (m_meteors.size() < 3 && QRandomGenerator::global()->generateDouble() < 0.02) {
        spawnMeteor();
    }

    for (int i = m_meteors.size() - 1; i >= 0; --i) {
        m_meteors[i].x += std::cos(m_meteors[i].angle) * m_meteors[i].speed;
        m_meteors[i].y += std::sin(m_meteors[i].angle) * m_meteors[i].speed;
        m_meteors[i].life += 0.033;
        if (m_meteors[i].life >= m_meteors[i].maxLife) {
            m_meteors.removeAt(i);
        }
    }

    update();
}

void StarBackground::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
}

void StarBackground::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    for (const Star &s : m_stars) {
        double twinkle = 0.5 + 0.5 * std::sin(m_time * s.twinkleSpeed + s.twinklePhase);
        double alpha = s.opacity * twinkle;
        int a = static_cast<int>(alpha * 255);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, a));
        double sx = s.x * w;
        double sy = s.y * h;
        painter.drawEllipse(QPointF(sx, sy), s.radius, s.radius);

        if (s.radius > 1.2 && twinkle > 0.7) {
            int glowA = static_cast<int>(alpha * 60);
            painter.setBrush(QColor(180, 200, 255, glowA));
            painter.drawEllipse(QPointF(sx, sy), s.radius * 3, s.radius * 3);
        }
    }

    for (const Meteor &m : m_meteors) {
        double lifeRatio = m.life / m.maxLife;
        double fadeAlpha = m.opacity * (1.0 - lifeRatio);

        double mx = m.x * w;
        double my = m.y * h;
        double tailX = mx - std::cos(m.angle) * m.length * w;
        double tailY = my - std::sin(m.angle) * m.length * h;

        QLinearGradient gradient(mx, my, tailX, tailY);
        gradient.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(fadeAlpha * 255)));
        gradient.setColorAt(0.3, QColor(200, 220, 255, static_cast<int>(fadeAlpha * 180)));
        gradient.setColorAt(1.0, QColor(100, 150, 255, 0));

        painter.setPen(QPen(QBrush(gradient), 2.0));
        painter.drawLine(QPointF(mx, my), QPointF(tailX, tailY));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, static_cast<int>(fadeAlpha * 200)));
        painter.drawEllipse(QPointF(mx, my), 2.0, 2.0);
    }
}
