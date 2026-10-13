#include <QApplication>
#include <QDir>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QWidget>

extern "C" {
#include "demo.h"
#include "spectrum/ui/cell_cursor.h"
}

class SpaceDemo final : public QWidget {
public:
    SpaceDemo() {
        setWindowTitle("Espacios de juego");
        setAccessibleName("Demostrador de navegacion y vistas privadas");
        setFocusPolicy(Qt::StrongFocus);
        setFixedSize(768, 640);
        reset(0);
    }

    void reset(uint8_t hex) {
        demo_reset(hex);
        player_ = 0;
        spectrum_cell_cursor_home(demo_first());
        update();
    }

    QPolygonF shape(space_cell_t cell) const {
        const qreal x = demo_x(cell) * 3.0;
        const qreal y = demo_y(cell) * 3.0 + 48.0;
        if (!demo_hexagonal()) {
            return QPolygonF(QRectF(x + 1, y + 1, 46, 46));
        }
        return QPolygonF({QPointF(x + 24, y), QPointF(x + 48, y + 12),
                          QPointF(x + 48, y + 36), QPointF(x + 24, y + 48),
                          QPointF(x, y + 36), QPointF(x, y + 12)});
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#f5f3ec"));
        painter.setPen(QColor("#152c3c"));
        painter.setFont(QFont("Sans Serif", 11));
        painter.drawText(24, 25, QString("%1   |   Vista %2")
            .arg(demo_hexagonal() ? "Hexagonos: 36 posiciones y un hueco" : "Rectangulo: 12 x 10")
            .arg(player_ ? "B" : "A"));
        for (space_cell_t cell = 0; cell < demo_extent(); ++cell) {
            if (!demo_valid(cell)) {
                continue;
            }
            const auto value = demo_view(cell, player_);
            const bool cursor = cell == spectrum_cell_cursor_current();
            painter.setPen(QPen(cursor ? QColor("#c74720") : QColor("#81919c"), cursor ? 3 : 1));
            painter.setBrush(value ? QColor("#dce9df") : QColor("#e5e9ed"));
            const auto polygon = shape(cell);
            painter.drawPolygon(polygon);
            painter.setPen(QColor("#152c3c"));
            painter.drawText(polygon.boundingRect(), Qt::AlignCenter,
                             value ? QString::number(value) : QString("?"));
        }
        painter.drawText(24, 605, "Flechas / QAOP: mover   E/Z: diagonales   Espacio / clic: revelar");
        painter.drawText(24, 629, "1: rectangulo   2: hexagonos   V: cambiar vista   ?: informacion oculta");
    }

    void keyPressEvent(QKeyEvent *event) override {
        uint8_t key = 0;
        switch (event->key()) {
        case Qt::Key_1: reset(0); return;
        case Qt::Key_2: reset(1); return;
        case Qt::Key_V: player_ ^= 1u; update(); return;
        case Qt::Key_Space:
        case Qt::Key_Return:
            demo_activate(spectrum_cell_cursor_current(), player_);
            update(); return;
        case Qt::Key_Up: key = 0x81u; break;
        case Qt::Key_Down: key = 0x82u; break;
        case Qt::Key_Left: key = 0x83u; break;
        case Qt::Key_Right: key = 0x84u; break;
        default:
            if (event->text().size() == 1) {
                key = static_cast<uint8_t>(event->text().toLower().at(0).toLatin1());
            }
        }
        spectrum_cell_cursor_move(key);
        update();
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) {
            return;
        }
        for (space_cell_t cell = 0; cell < demo_extent(); ++cell) {
            if (demo_valid(cell) && shape(cell).containsPoint(event->localPos(), Qt::OddEvenFill)) {
                spectrum_cell_cursor_home(cell);
                demo_activate(cell, player_);
                update();
                return;
            }
        }
    }

private:
    uint8_t player_ = 0;
};

static void require(bool condition) {
    if (!condition) {
        qFatal("Space demo interaction contract failed");
    }
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    SpaceDemo window;
    if (app.arguments().contains("--self-test")) {
        const QString output = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QDir::currentPath();
        require(QDir().mkpath(output));
        auto press = [&](int key) {
            QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
            QApplication::sendEvent(&window, &event);
        };
        auto click = [&](space_cell_t cell) {
            const QPointF point = window.shape(cell).boundingRect().center();
            QMouseEvent event(QEvent::MouseButtonPress, point, point, point,
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&window, &event);
        };
        press(Qt::Key_Right);
        require(spectrum_cell_cursor_current() == 1 && demo_view(1, 0) == 0);
        press(Qt::Key_Space);
        require(demo_view(1, 0) != 0);
        click(3);
        require(spectrum_cell_cursor_current() == 3 && demo_view(3, 0) != 0);
        require(window.grab().save(output + "/rectangle.png"));
        press(Qt::Key_2);
        click(24);
        require(spectrum_cell_cursor_current() == demo_first());
        require(window.grab().save(output + "/hexagon-a.png"));
        press(Qt::Key_V);
        click(4);
        require(demo_view(4, 1) != 0 && spectrum_cell_cursor_current() == 4);
        require(window.grab().save(output + "/hexagon-b.png"));
        qInfo("Space demo keyboard, pointer, hole and player-view tests passed");
        return 0;
    }
    window.show();
    return app.exec();
}
