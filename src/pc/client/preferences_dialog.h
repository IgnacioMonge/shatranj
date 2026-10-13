#ifndef SHATRANJ_PREFERENCES_DIALOG_H
#define SHATRANJ_PREFERENCES_DIALOG_H

#include "engine_installer.h"
#include "manual_index.h"
#include "preferences.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTabWidget;

// Modal Preferences: Board, Player and Engine tabs. Changes apply on OK.
class PreferencesDialog final : public QDialog {
public:
    enum Tab { BoardTab, PlayerTab, EngineTab };

    // sessionLocked: Player and Engine are read-only during a session.
    PreferencesDialog(const ClientPreferences &prefs, bool sessionLocked,
                      QWidget *parent = nullptr);

    ClientPreferences preferences() const;
    void showTab(Tab tab);

    // Opens the manual at a section ("How does it work?", F1).
    std::function<void(ManualSection)> onHelp;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QWidget *buildBoardTab(const ClientPreferences &prefs);
    QWidget *buildPlayerTab(const ClientPreferences &prefs);
    QWidget *buildEngineTab(const ClientPreferences &prefs);
    void refreshEngine(const QString &message = QString());
    void setBusy(bool busy);
    void installLatest();
    void uninstallEngine();
    void locateEngine();
    void adoptEngine(const QString &path);

    QTabWidget *tabs_ = nullptr;
    QComboBox *board_ = nullptr;
    QComboBox *pieces_ = nullptr;
    QCheckBox *hints_ = nullptr;
    QLineEdit *name_ = nullptr;
    QLabel *engineStatus_ = nullptr;
    QPushButton *install_ = nullptr;
    QPushButton *uninstall_ = nullptr;
    QPushButton *locate_ = nullptr;
    QProgressBar *progress_ = nullptr;
    QCheckBox *shown_ = nullptr;
    QSpinBox *level_ = nullptr;
    QSpinBox *delay_ = nullptr;
    EngineInstaller installer_;
    QString enginePath_;
    QString engineName_;
    bool sessionLocked_ = false;
    bool enginePlays_ = false;
    bool busy_ = false;
};

#endif // SHATRANJ_PREFERENCES_DIALOG_H
