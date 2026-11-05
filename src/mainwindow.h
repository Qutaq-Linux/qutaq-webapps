#pragma once

#include <QMainWindow>

class QLineEdit;
class QTreeView;
class QLabel;
class QStackedWidget;
class QToolButton;
class QTimer;
class QFileSystemWatcher;
class WebAppModel;
class WebAppFilterModel;
struct WebApp;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    QString searchText() const;
    void setSearchText(const QString &text);

signals:
    // Пользователь выбрал другой язык — окно нужно пересоздать.
    void languageChanged();

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void closeEvent(QCloseEvent *e) override;

private:
    void reload(const QString &selectCodename = {});
    void updateState();
    int currentSourceRow() const;
    void selectCodename(const QString &codename);

    void addApp();
    void editApp();
    void removeApp();
    void launchApp();

    QLineEdit *m_search;
    QTreeView *m_view;
    QStackedWidget *m_stack;
    QLabel *m_empty;
    QLabel *m_counter;
    QToolButton *m_addBtn, *m_removeBtn, *m_editBtn, *m_runBtn;
    WebAppModel *m_model;
    WebAppFilterModel *m_proxy;
    QFileSystemWatcher *m_watcher;
    QTimer *m_reloadTimer;
};
