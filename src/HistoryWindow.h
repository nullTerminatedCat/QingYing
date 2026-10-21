#pragma once
#include <QDialog>

class QListWidget;

class HistoryWindow : public QDialog {
    Q_OBJECT
public:
    explicit HistoryWindow(QWidget* parent = nullptr);
    void reload();

private:
    QString currentPath() const;
    QListWidget* m_list;
};
