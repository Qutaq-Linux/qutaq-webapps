#pragma once

#include "webapp.h"

#include <QDialog>
#include <QImage>

class QLineEdit;
class QComboBox;
class QToolButton;
class QPushButton;
class QLabel;
class ToggleSwitch;
class FaviconFetcher;

class WebAppDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WebAppDialog(QWidget *parent = nullptr);
    void setWebApp(const WebApp &app);     // режим редактирования
    WebApp webApp() const { return m_app; } // результат после accept()

    void accept() override;

private:
    void validate();
    void normalizeUrl();
    void fetchFavicon();
    void onFaviconsFetched(const QList<QImage> &icons, const QString &title);
    void chooseIconFile();
    void chooseThemeIcon();
    void setPendingImage(const QImage &img);
    void setIconValue(const QString &icon);
    void updateBrowserDependent();

    WebApp m_app;
    bool m_editing = false;
    bool m_iconChosenByUser = false;
    QImage m_pendingImage;   // скачанный значок, будет сохранён при OK
    QString m_iconValue;     // путь/имя значка из темы
    QList<QImage> m_fetched;

    QLineEdit *m_name;
    QLineEdit *m_description;
    QLineEdit *m_url;
    QToolButton *m_fetchBtn;
    QToolButton *m_iconBtn;
    QComboBox *m_category;
    QComboBox *m_browser;
    QLineEdit *m_params;
    ToggleSwitch *m_navbar;
    ToggleSwitch *m_private;
    ToggleSwitch *m_isolated;
    QLabel *m_isolatedLabel;
    QLabel *m_status;
    QPushButton *m_ok;
    FaviconFetcher *m_fetcher;
};
