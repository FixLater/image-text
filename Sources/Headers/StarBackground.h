#ifndef STARBACKGROUND_H
#define STARBACKGROUND_H

#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QPainter>
#include <QRandomGenerator>

struct Star {
    double x;
    double y;
    double radius;
    double opacity;
    double twinkleSpeed;
    double twinklePhase;
};

struct Meteor {
    double x;
    double y;
    double length;
    double speed;
    double angle;
    double opacity;
    double life;
    double maxLife;
};

class StarBackground : public QWidget {
    Q_OBJECT

public:
    explicit StarBackground(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void initStars();
    void spawnMeteor();
    void updateAnimation();

    QVector<Star> m_stars;
    QVector<Meteor> m_meteors;
    QTimer *m_timer;
    double m_time = 0.0;
    int m_maxStars = 120;
};

#endif // STARBACKGROUND_H
