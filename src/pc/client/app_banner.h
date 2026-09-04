#ifndef APP_BANNER_H
#define APP_BANNER_H

#include <QWidget>
#include <QImage>
#include <functional>

class QKeyEvent;
class QResizeEvent;

class AppBanner : public QWidget {
    Q_DISABLE_COPY_MOVE(AppBanner)
public:
    explicit AppBanner(QWidget *parent = nullptr);

    std::function<void()> clicked;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void paintEvent(QPaintEvent *) override;
    void ensureImages();
    void ensureBoardStrip();

    QImage mosaicImage_;
    QImage wordmarkImage_;
    QImage boardImage_;
    QImage boardStrip_;
    bool imagesLoaded_ = false;
    bool pressed_ = false;
};

#endif // APP_BANNER_H
